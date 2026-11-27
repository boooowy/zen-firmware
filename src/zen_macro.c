/*
 * Copyright (c) 2026 E24-GH
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zen_behavior_macro

#include "zen/macro.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>

LOG_MODULE_REGISTER(zen_macro, CONFIG_ZMK_LOG_LEVEL);

/* How long a TAP holds the key, and the gap left after a step that does not
 * say otherwise. Fast enough that a long string is not a wait, slow
 * enough that hosts keep up; a macro that needs more can carry a WAIT step. */
#define ZEN_MACRO_TAP_MS 15
#define ZEN_MACRO_GAP_MS 15

struct zen_macro_slot {
    uint8_t count;
    struct zen_macro_step steps[CONFIG_ZEN_RUNTIME_MACRO_STEPS];
};

static struct zen_macro_slot slots[CONFIG_ZEN_RUNTIME_MACRO_SLOTS];

int zen_macro_slot_count(void) { return ARRAY_SIZE(slots); }

int zen_macro_max_steps(void) { return CONFIG_ZEN_RUNTIME_MACRO_STEPS; }

/* A macro only ever sends key presses, so one &kp is all it needs. Looked up
 * once: the name is matched against every registered behavior. */
static const struct device *key_press_device(void) {
    static const struct device *cached;
    if (!cached) {
        cached = zmk_behavior_get_binding("key_press");
    }
    return cached;
}

/* Rejects a macro that would leave a key held down. Nothing else in the
 * firmware would ever release it, so the keyboard would look broken until it
 * was unplugged -- worse than refusing to save. */
static int validate(const struct zen_macro_step *steps, size_t count) {
    int held = 0;

    for (size_t i = 0; i < count; i++) {
        switch (steps[i].op) {
        case ZEN_MACRO_OP_TAP:
        case ZEN_MACRO_OP_WAIT:
            break;
        case ZEN_MACRO_OP_PRESS:
            held++;
            break;
        case ZEN_MACRO_OP_RELEASE:
            if (held == 0) {
                LOG_WRN("Macro step %u releases a key that was never pressed", (unsigned)i);
                return -EINVAL;
            }
            held--;
            break;
        default:
            LOG_WRN("Macro step %u has op %u, which is not one this firmware knows",
                    (unsigned)i, steps[i].op);
            return -EINVAL;
        }
    }

    if (held != 0) {
        LOG_WRN("Macro leaves %d key(s) held down", held);
        return -EINVAL;
    }
    return 0;
}

int zen_macro_set(size_t slot, const struct zen_macro_step *steps, size_t count) {
    if (slot >= ARRAY_SIZE(slots) || count > CONFIG_ZEN_RUNTIME_MACRO_STEPS) {
        return -EINVAL;
    }
    if (count > 0 && steps == NULL) {
        return -EINVAL;
    }

    int rc = validate(steps, count);
    if (rc < 0) {
        return rc;
    }

    struct zen_macro_slot next = {.count = count};
    for (size_t i = 0; i < count; i++) {
        next.steps[i] = steps[i];
    }
    slots[slot] = next;
    return 0;
}

int zen_macro_get(size_t slot, struct zen_macro_step *steps, size_t *count) {
    if (slot >= ARRAY_SIZE(slots)) {
        return -EINVAL;
    }
    if (count) {
        *count = slots[slot].count;
    }
    if (steps) {
        for (size_t i = 0; i < slots[slot].count; i++) {
            steps[i] = slots[slot].steps[i];
        }
    }
    return 0;
}

/* --- Playback ------------------------------------------------------------ */

/* Every step is queued the moment the macro is triggered, and the queue takes
 * a copy of each binding. That is what makes editing a slot mid-playback
 * harmless, and it is why this has no -EBUSY of its own. */
static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    size_t slot = binding->param1;
    if (slot >= ARRAY_SIZE(slots)) {
        LOG_WRN("No macro slot %u", (unsigned)slot);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    const struct device *kp = key_press_device();
    if (!kp) {
        LOG_ERR("No key_press behavior to play a macro through");
        return ZMK_BEHAVIOR_OPAQUE;
    }

    const struct zen_macro_slot *macro = &slots[slot];
    for (size_t i = 0; i < macro->count; i++) {
        const struct zen_macro_step *step = &macro->steps[i];
        if (step->op == ZEN_MACRO_OP_WAIT) {
            /* Folded into the step before it, below. A WAIT in first place
             * has nothing to delay and is simply skipped. */
            continue;
        }

        uint32_t gap = ZEN_MACRO_GAP_MS;
        if (i + 1 < macro->count && macro->steps[i + 1].op == ZEN_MACRO_OP_WAIT) {
            gap = macro->steps[i + 1].value;
        }

        struct zmk_behavior_binding key = {
            .behavior_dev = kp->name,
            .param1 = step->value,
            .param2 = 0,
        };

        switch (step->op) {
        case ZEN_MACRO_OP_TAP:
            zmk_behavior_queue_add(&event, key, true, ZEN_MACRO_TAP_MS);
            zmk_behavior_queue_add(&event, key, false, gap);
            break;
        case ZEN_MACRO_OP_PRESS:
            zmk_behavior_queue_add(&event, key, true, gap);
            break;
        case ZEN_MACRO_OP_RELEASE:
            zmk_behavior_queue_add(&event, key, false, gap);
            break;
        default:
            break;
        }
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    /* A macro fires once, on press. Holding the key does not repeat it. */
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api zen_macro_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

static int zen_macro_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define ZEN_MACRO_INST(n)                                                                          \
    BEHAVIOR_DT_INST_DEFINE(n, zen_macro_init, NULL, NULL, NULL, POST_KERNEL,                      \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &zen_macro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZEN_MACRO_INST)

/* --- Persistence --------------------------------------------------------- */

#define ZEN_MACRO_SETTINGS_ROOT "zen_macro"
#define ZEN_MACRO_SETTINGS_KEY "slots"
#define ZEN_MACRO_SETTINGS_PATH ZEN_MACRO_SETTINGS_ROOT "/" ZEN_MACRO_SETTINGS_KEY

#define ZEN_MACRO_SETTINGS_VERSION 1

/* Entries are variable length -- a slot costs only the steps it holds, which
 * keeps the usual record small. The header says how long a step is, so a
 * later firmware can refuse a record it would misread rather than guess. */
struct zen_macro_settings_header {
    uint8_t version;
    uint8_t entry_count;
    uint8_t step_size;
    uint8_t reserved;
} __packed;

struct zen_macro_settings_entry {
    uint8_t slot;
    uint8_t count;
    uint8_t reserved[2];
    /* followed by `count` steps */
} __packed;

#define ZEN_MACRO_SETTINGS_MAX_BYTES                                                               \
    (sizeof(struct zen_macro_settings_header) +                                                    \
     CONFIG_ZEN_RUNTIME_MACRO_SLOTS *                                                              \
         (sizeof(struct zen_macro_settings_entry) +                                                \
          CONFIG_ZEN_RUNTIME_MACRO_STEPS * sizeof(struct zen_macro_step)))

static int zen_macro_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                  void *cb_arg) {
    const char *next;

    if (!settings_name_steq(name, ZEN_MACRO_SETTINGS_KEY, &next) || next) {
        return -ENOENT;
    }
    if (len < sizeof(struct zen_macro_settings_header) || len > ZEN_MACRO_SETTINGS_MAX_BYTES) {
        LOG_WRN("Saved macros are %u bytes, which is not a record this firmware can read",
                (unsigned)len);
        return -EINVAL;
    }

    /* static, not on the stack: settings_load() runs on the main thread, whose
     * stack is CONFIG_MAIN_STACK_SIZE (1024 on this board). A record buffer of
     * a few hundred bytes there is a stack overflow waiting for the day the
     * record gets big enough, and the symptom is a board that reboots forever
     * and looks dead. Nothing here is re-entrant: settings load once at boot,
     * and a save comes from the bridge thread, one request at a time. */
    static uint8_t buffer[ZEN_MACRO_SETTINGS_MAX_BYTES];
    int read = read_cb(cb_arg, buffer, len);
    if (read < (int)sizeof(struct zen_macro_settings_header)) {
        return read < 0 ? read : -EINVAL;
    }

    const struct zen_macro_settings_header *header =
        (const struct zen_macro_settings_header *)buffer;
    if (header->version != ZEN_MACRO_SETTINGS_VERSION ||
        header->step_size != sizeof(struct zen_macro_step)) {
        LOG_WRN("Saved macros are version %u/%u-byte steps; this firmware reads %u/%u",
                header->version, header->step_size, ZEN_MACRO_SETTINGS_VERSION,
                (unsigned)sizeof(struct zen_macro_step));
        return -EINVAL;
    }

    size_t offset = sizeof(*header);
    size_t restored = 0;
    for (size_t i = 0; i < header->entry_count; i++) {
        if (offset + sizeof(struct zen_macro_settings_entry) > (size_t)read) {
            break;
        }
        const struct zen_macro_settings_entry *entry =
            (const struct zen_macro_settings_entry *)&buffer[offset];
        offset += sizeof(*entry);

        size_t bytes = (size_t)entry->count * sizeof(struct zen_macro_step);
        if (offset + bytes > (size_t)read) {
            break;
        }
        const struct zen_macro_step *steps = (const struct zen_macro_step *)&buffer[offset];
        offset += bytes;

        if (zen_macro_set(entry->slot, steps, entry->count) == 0) {
            restored++;
        } else {
            LOG_WRN("Dropped the saved macro in slot %u", entry->slot);
        }
    }

    LOG_INF("Restored %u of %u saved macros", (unsigned)restored, header->entry_count);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zen_macro, ZEN_MACRO_SETTINGS_ROOT, NULL, zen_macro_settings_set,
                               NULL, NULL);

int zen_macro_settings_save(void) {
    /* static, not on the stack: settings_load() runs on the main thread, whose
     * stack is CONFIG_MAIN_STACK_SIZE (1024 on this board). A record buffer of
     * a few hundred bytes there is a stack overflow waiting for the day the
     * record gets big enough, and the symptom is a board that reboots forever
     * and looks dead. Nothing here is re-entrant: settings load once at boot,
     * and a save comes from the bridge thread, one request at a time. */
    static uint8_t buffer[ZEN_MACRO_SETTINGS_MAX_BYTES];
    memset(buffer, 0, sizeof(buffer));

    struct zen_macro_settings_header *header = (struct zen_macro_settings_header *)buffer;
    header->version = ZEN_MACRO_SETTINGS_VERSION;
    header->step_size = sizeof(struct zen_macro_step);

    size_t offset = sizeof(*header);
    size_t count = 0;
    for (size_t slot = 0; slot < ARRAY_SIZE(slots); slot++) {
        if (slots[slot].count == 0) {
            continue;
        }

        struct zen_macro_settings_entry *entry = (struct zen_macro_settings_entry *)&buffer[offset];
        offset += sizeof(*entry);
        entry->slot = slot;
        entry->count = slots[slot].count;

        size_t bytes = (size_t)slots[slot].count * sizeof(struct zen_macro_step);
        memcpy(&buffer[offset], slots[slot].steps, bytes);
        offset += bytes;
        count++;
    }
    header->entry_count = count;

    int rc = settings_save_one(ZEN_MACRO_SETTINGS_PATH, buffer, offset);
    if (rc < 0) {
        LOG_ERR("Could not save macros: %d", rc);
        return rc;
    }

    LOG_INF("Saved %u macros (%u bytes)", (unsigned)count, (unsigned)offset);
    return 0;
}

int zen_macro_settings_reset(void) {
    for (size_t slot = 0; slot < ARRAY_SIZE(slots); slot++) {
        slots[slot].count = 0;
    }

    int rc = settings_delete(ZEN_MACRO_SETTINGS_PATH);
    if (rc < 0) {
        LOG_ERR("Could not remove the saved macros: %d", rc);
        return rc;
    }

    LOG_INF("Cleared every macro");
    return 0;
}
