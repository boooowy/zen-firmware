/*
 * Copyright (c) 2026 E24-GH
 *
 * SPDX-License-Identifier: MIT
 */

#include "zen/combo_settings.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <zmk/behavior.h>

#include "zen/runtime_combo.h"
#include "zen_config_wire.h"

LOG_MODULE_REGISTER(zen_combo_settings, CONFIG_ZMK_LOG_LEVEL);

#define ZEN_COMBO_SETTINGS_ROOT "zen_combo"
#define ZEN_COMBO_SETTINGS_KEY "slots"
#define ZEN_COMBO_SETTINGS_PATH ZEN_COMBO_SETTINGS_ROOT "/" ZEN_COMBO_SETTINGS_KEY

#define ZEN_COMBO_SETTINGS_VERSION 1

/* Fixed, not CONFIG_ZEN_RUNTIME_COMBO_MAX_KEYS: the Kconfig value may change
 * between firmware builds and the stored record has to keep its shape, or a
 * keyboard would lose its combos the first time that number moved. The same
 * reasoning puts the slot index inside each entry rather than relying on the
 * entries' order. */
#define ZEN_COMBO_SETTINGS_MAX_KEYS 8u

struct zen_combo_settings_entry {
    uint8_t slot;
    uint8_t key_count;
    uint16_t timeout_ms;
    uint32_t layer_mask;
    uint16_t key_positions[ZEN_COMBO_SETTINGS_MAX_KEYS];
    char behavior[ZEN_CONFIG_WIRE_BEHAVIOR_NAME_LEN];
    int32_t param1;
    int32_t param2;
} __packed;

struct zen_combo_settings_record {
    uint8_t version;
    uint8_t entry_count;
    uint8_t entry_size; /* so a later version can be skipped, not misread */
    uint8_t reserved;
    struct zen_combo_settings_entry entries[];
} __packed;

/* Only the occupied slots are written, so the usual record is far smaller. */
#define ZEN_COMBO_SETTINGS_MAX_BYTES                                                               \
    (sizeof(struct zen_combo_settings_record) +                                                    \
     CONFIG_ZEN_RUNTIME_COMBO_SLOTS * sizeof(struct zen_combo_settings_entry))

static int apply_entry(const struct zen_combo_settings_entry *entry) {
    if (entry->slot >= (uint8_t)zen_runtime_combo_slot_count()) {
        LOG_WRN("Saved combo names slot %u, which this firmware does not have", entry->slot);
        return -EINVAL;
    }
    if (entry->key_count > ZEN_COMBO_SETTINGS_MAX_KEYS ||
        entry->key_count > (uint8_t)zen_runtime_combo_max_keys()) {
        LOG_WRN("Saved combo in slot %u has %u keys, more than this firmware holds", entry->slot,
                entry->key_count);
        return -EINVAL;
    }

    char name[ZEN_CONFIG_WIRE_BEHAVIOR_NAME_LEN];
    memcpy(name, entry->behavior, sizeof(name));
    name[sizeof(name) - 1] = '\0';

    const struct device *behavior_dev = zmk_behavior_get_binding(name);
    if (!behavior_dev) {
        /* The behavior was compiled out since the combo was saved. Dropping
         * just this one is better than refusing the whole record. */
        LOG_WRN("Saved combo in slot %u wants behavior '%s', which is not in this firmware",
                entry->slot, name);
        return -ENODEV;
    }

    int32_t positions[ZEN_COMBO_SETTINGS_MAX_KEYS];
    for (size_t i = 0; i < entry->key_count; i++) {
        positions[i] = entry->key_positions[i];
    }

    struct zmk_behavior_binding binding = {
        .behavior_dev = behavior_dev->name,
        .param1 = entry->param1,
        .param2 = entry->param2,
    };

    return zen_runtime_combo_set(entry->slot, positions, entry->key_count, entry->layer_mask,
                                 entry->timeout_ms, &binding);
}

static int zen_combo_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                  void *cb_arg) {
    const char *next;

    if (!settings_name_steq(name, ZEN_COMBO_SETTINGS_KEY, &next) || next) {
        return -ENOENT;
    }
    if (len < sizeof(struct zen_combo_settings_record) || len > ZEN_COMBO_SETTINGS_MAX_BYTES) {
        LOG_WRN("Saved combos are %u bytes, which is not a record this firmware can read",
                (unsigned)len);
        return -EINVAL;
    }

    /* static, not on the stack: settings_load() runs on the main thread, whose
     * stack is CONFIG_MAIN_STACK_SIZE (1024 on this board). A record buffer of
     * a few hundred bytes there is a stack overflow waiting for the day the
     * record gets big enough, and the symptom is a board that reboots forever
     * and looks dead. Nothing here is re-entrant: settings load once at boot,
     * and a save comes from the bridge thread, one request at a time. */
    static uint8_t buffer[ZEN_COMBO_SETTINGS_MAX_BYTES];
    int read = read_cb(cb_arg, buffer, len);
    if (read < (int)sizeof(struct zen_combo_settings_record)) {
        return read < 0 ? read : -EINVAL;
    }

    const struct zen_combo_settings_record *record =
        (const struct zen_combo_settings_record *)buffer;
    if (record->version != ZEN_COMBO_SETTINGS_VERSION ||
        record->entry_size != sizeof(struct zen_combo_settings_entry)) {
        LOG_WRN("Saved combos are version %u/%u-byte entries; this firmware reads %u/%u",
                record->version, record->entry_size, ZEN_COMBO_SETTINGS_VERSION,
                (unsigned)sizeof(struct zen_combo_settings_entry));
        return -EINVAL;
    }

    size_t available = ((size_t)read - sizeof(struct zen_combo_settings_record)) /
                       sizeof(struct zen_combo_settings_entry);
    size_t count = MIN((size_t)record->entry_count, available);

    size_t applied = 0;
    for (size_t i = 0; i < count; i++) {
        if (apply_entry(&record->entries[i]) == 0) {
            applied++;
        }
    }

    LOG_INF("Restored %u of %u saved combos", (unsigned)applied, (unsigned)count);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zen_combo, ZEN_COMBO_SETTINGS_ROOT, NULL, zen_combo_settings_set,
                               NULL, NULL);

int zen_combo_settings_save(void) {
    /* static, not on the stack: settings_load() runs on the main thread, whose
     * stack is CONFIG_MAIN_STACK_SIZE (1024 on this board). A record buffer of
     * a few hundred bytes there is a stack overflow waiting for the day the
     * record gets big enough, and the symptom is a board that reboots forever
     * and looks dead. Nothing here is re-entrant: settings load once at boot,
     * and a save comes from the bridge thread, one request at a time. */
    static uint8_t buffer[ZEN_COMBO_SETTINGS_MAX_BYTES];
    memset(buffer, 0, sizeof(buffer));

    struct zen_combo_settings_record *record = (struct zen_combo_settings_record *)buffer;
    record->version = ZEN_COMBO_SETTINGS_VERSION;
    record->entry_size = sizeof(struct zen_combo_settings_entry);

    size_t count = 0;
    for (size_t slot = 0; slot < (size_t)zen_runtime_combo_slot_count(); slot++) {
        int32_t positions[ZEN_COMBO_SETTINGS_MAX_KEYS];
        size_t len = 0;
        uint32_t layer_mask = 0;
        int32_t timeout_ms = 0;
        struct zmk_behavior_binding binding = {0};

        if (zen_runtime_combo_get(slot, positions, &len, &layer_mask, &timeout_ms, &binding) < 0) {
            continue;
        }
        if (len == 0) {
            continue;
        }
        if (len > ZEN_COMBO_SETTINGS_MAX_KEYS) {
            /* Would be silently truncated otherwise, which is worse than
             * refusing: the keyboard would come back with a different combo
             * from the one on screen. */
            LOG_ERR("Combo in slot %u has %u keys, more than the saved format holds",
                    (unsigned)slot, (unsigned)len);
            return -EINVAL;
        }

        struct zen_combo_settings_entry *entry = &record->entries[count++];
        entry->slot = slot;
        entry->key_count = len;
        entry->timeout_ms = timeout_ms;
        entry->layer_mask = layer_mask;
        for (size_t i = 0; i < len; i++) {
            entry->key_positions[i] = positions[i];
        }
        if (binding.behavior_dev) {
            strncpy(entry->behavior, binding.behavior_dev, sizeof(entry->behavior) - 1);
        }
        entry->param1 = binding.param1;
        entry->param2 = binding.param2;
    }

    record->entry_count = count;

    size_t bytes =
        sizeof(struct zen_combo_settings_record) + count * sizeof(struct zen_combo_settings_entry);
    int rc = settings_save_one(ZEN_COMBO_SETTINGS_PATH, buffer, bytes);
    if (rc < 0) {
        LOG_ERR("Could not save combos: %d", rc);
        return rc;
    }

    LOG_INF("Saved %u combos (%u bytes)", (unsigned)count, (unsigned)bytes);
    return 0;
}

int zen_combo_settings_reset(void) {
    int first_error = 0;

    for (size_t slot = 0; slot < (size_t)zen_runtime_combo_slot_count(); slot++) {
        int rc = zen_runtime_combo_set(slot, NULL, 0, 0, 0, NULL);
        if (rc < 0 && first_error == 0) {
            first_error = rc;
        }
    }
    if (first_error != 0) {
        /* -EBUSY with keys down: leave the saved record alone so the keyboard
         * and the settings partition do not disagree. */
        return first_error;
    }

    int rc = settings_delete(ZEN_COMBO_SETTINGS_PATH);
    if (rc < 0) {
        LOG_ERR("Could not remove the saved combos: %d", rc);
        return rc;
    }

    LOG_INF("Cleared every editable combo");
    return 0;
}
