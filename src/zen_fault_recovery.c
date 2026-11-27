/*
 * Recover from a fatal error instead of halting forever.
 *
 * Zephyr's default k_sys_fatal_error_handler() calls arch_system_halt(), whose
 * weak implementation locks interrupts and spins:
 *
 *     (void)arch_irq_lock();
 *     for (;;) { }
 *
 * With CONFIG_WATCHDOG off -- which it is here -- nothing ever breaks out of
 * that loop. On a split half this reads to the user as a keyboard that is
 * simply dead: no LED, no keys, battery still full, and only cycling the power
 * switch brings it back. That is exactly the failure we spent weeks chasing on
 * the ZEN's peripheral half, and it persisted after deep sleep was disabled
 * (CONFIG_ZMK_IDLE_SLEEP_TIMEOUT raised to 7 days), which rules out the
 * sleep/wake path. See Obsidian: zen-left-unresponsive-after-sleep-usb-poll.
 *
 * So: reboot instead. A half that faults comes back on its own after a brief
 * interruption rather than staying dead until someone power-cycles it.
 *
 * The reason is stashed in __noinit RAM, which survives the warm reset, and
 * logged on the way back up together with RESETREAS. That is what tells us
 * afterwards whether a fault happened at all -- the thing we have had no way
 * to see so far.
 */

#include <zephyr/init.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include <hal/nrf_power.h>

LOG_MODULE_REGISTER(zen_fault_recovery, CONFIG_ZMK_LOG_LEVEL);

#define ZEN_FAULT_MAGIC 0x5A454E46 /* "ZENF" */

struct zen_fault_record {
    uint32_t magic;
    uint32_t reason;
    uint32_t count;
};

/* __noinit is left alone by the C startup code, so it survives the warm reset
 * we trigger below. Garbage on a cold boot -- hence the magic. */
static __noinit struct zen_fault_record fault_record;

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf) {
    ARG_UNUSED(esf);

    if (fault_record.magic != ZEN_FAULT_MAGIC) {
        fault_record.magic = ZEN_FAULT_MAGIC;
        fault_record.count = 0;
    }
    fault_record.reason = reason;
    fault_record.count++;

#if IS_ENABLED(CONFIG_LOG)
    /* Only defined when logging is actually compiled in; this build has it off. */
    LOG_PANIC();
#endif
    LOG_ERR("Fatal error %u, rebooting (fault #%u)", reason, fault_record.count);

    sys_reboot(SYS_REBOOT_WARM);
    CODE_UNREACHABLE;
}

uint32_t zen_fault_recovery_count(void) {
    return fault_record.magic == ZEN_FAULT_MAGIC ? fault_record.count : 0;
}

uint32_t zen_fault_recovery_last_reason(void) {
    return fault_record.magic == ZEN_FAULT_MAGIC ? fault_record.reason : 0;
}

static int zen_fault_recovery_init(void) {
    /* RESETREAS bits are sticky until written back, so clear what we read or
     * every later boot inherits this one's reasons. */
    const uint32_t resetreas = nrf_power_resetreas_get(NRF_POWER);
    nrf_power_resetreas_clear(NRF_POWER, resetreas);

    LOG_INF("Boot: RESETREAS=0x%08x%s%s%s%s%s", resetreas,
            (resetreas & NRF_POWER_RESETREAS_RESETPIN_MASK) ? " pin" : "",
            (resetreas & NRF_POWER_RESETREAS_DOG_MASK) ? " watchdog" : "",
            (resetreas & NRF_POWER_RESETREAS_SREQ_MASK) ? " soft" : "",
            (resetreas & NRF_POWER_RESETREAS_LOCKUP_MASK) ? " lockup" : "",
            (resetreas & NRF_POWER_RESETREAS_OFF_MASK) ? " wake-from-off" : "");

    if (zen_fault_recovery_count() > 0) {
        LOG_ERR("Recovered from fatal error %u (fault #%u since last cold boot)",
                fault_record.reason, fault_record.count);
    }

    return 0;
}

SYS_INIT(zen_fault_recovery_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
