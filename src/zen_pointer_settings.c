// SPDX-License-Identifier: GPL-2.0-or-later

#include "zen_pointer_settings.h"

#include "zen_config_codec.h"

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(zen_pointer_settings, CONFIG_ZMK_LOG_LEVEL);

#define ZEN_POINTER_SETTINGS_ROOT "zen_pointer"
#define ZEN_POINTER_SETTINGS_KEY "settings"

static struct zen_pointer_settings_payload active_payload;
static bool payload_loaded;

static const struct zen_pointer_settings_payload default_payload = {
    .version = ZEN_POINTER_SETTINGS_VERSION,
    .pointer_scale_percent = ZEN_POINTER_SETTINGS_SCALE_DEFAULT,
    .scroll_scale_percent = ZEN_POINTER_SETTINGS_SCALE_DEFAULT,
    .invert_x = false,
    .invert_y = false,
    .invert_scroll = false,
    .rotation_degrees = ZEN_POINTER_SETTINGS_ROTATION_DEFAULT,
    .auto_mouse_enabled = ZEN_POINTER_SETTINGS_AUTO_MOUSE_ENABLED_DEFAULT,
    .auto_mouse_timeout_ms = ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_DEFAULT,
    .auto_mouse_idle_ms = ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_DEFAULT,
};

static void reset_payload_defaults(void) { active_payload = default_payload; }

static int persist_payload(void) {
    return settings_save_one(ZEN_POINTER_SETTINGS_ROOT "/" ZEN_POINTER_SETTINGS_KEY, &active_payload,
                             sizeof(active_payload));
}

static int zen_pointer_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                    void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, ZEN_POINTER_SETTINGS_KEY, &next) && !next) {
        /* Start from the defaults so a record written by an older firmware,
         * which is shorter than the struct is now, leaves the fields it never
         * knew about at something sane rather than at whatever was on the
         * stack. */
        struct zen_pointer_settings_payload payload = default_payload;

        if (len > sizeof(payload)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &payload, len);
        if (rc < 0) {
            return rc;
        }

        /* Fill in what each version added, oldest first. Doing it step by step
         * rather than "anything older than current gets defaults" matters: a
         * version-2 record carries a rotation the user chose, and blanket
         * defaulting would throw it away on the upgrade to 3.
         *
         * Version 1 is the awkward one. It happens to be the same size as
         * version 2 -- rotation_degrees landed in what used to be tail padding
         * -- so the length check above cannot tell them apart, and the byte
         * read into rotation_degrees is whatever padding the old build wrote.
         * The version byte is the only thing that distinguishes them. */
        if (payload.version < 2) {
            payload.rotation_degrees = ZEN_POINTER_SETTINGS_ROTATION_DEFAULT;
        }
        if (payload.version < 3) {
            payload.auto_mouse_enabled = ZEN_POINTER_SETTINGS_AUTO_MOUSE_ENABLED_DEFAULT;
            payload.auto_mouse_timeout_ms = ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_DEFAULT;
            payload.auto_mouse_idle_ms = ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_DEFAULT;
        }
        payload.version = ZEN_POINTER_SETTINGS_VERSION;

        active_payload = payload;
        return 0;
    }

    return -ENOENT;
}

static int zen_pointer_settings_commit(void) {
    if (!zen_pointer_settings_is_valid_payload(&active_payload)) {
        LOG_WRN("Loaded pointer settings were invalid, using defaults");
        reset_payload_defaults();
    }

    payload_loaded = true;
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zen_pointer, ZEN_POINTER_SETTINGS_ROOT, NULL,
                               zen_pointer_settings_set, zen_pointer_settings_commit, NULL);

const struct zen_pointer_settings_payload *zen_pointer_settings_payload(void) {
    return &active_payload;
}

bool zen_pointer_settings_loaded(void) { return payload_loaded; }

int zen_pointer_settings_copy_payload(struct zen_pointer_settings_payload *payload_out) {
    if (!payload_out) {
        return -EINVAL;
    }

    *payload_out = active_payload;
    return 0;
}

int zen_pointer_settings_reset_to_defaults(void) {
    reset_payload_defaults();
    payload_loaded = true;
    return persist_payload();
}

int zen_pointer_settings_apply_payload(const struct zen_pointer_settings_payload *payload,
                                       bool persist) {
    if (!zen_pointer_settings_is_valid_payload(payload)) {
        return -EINVAL;
    }

    active_payload = *payload;
    payload_loaded = true;

    if (!persist) {
        return 0;
    }

    return persist_payload();
}

static int zen_pointer_settings_init(void) {
    reset_payload_defaults();
    payload_loaded = false;
    return 0;
}

SYS_INIT(zen_pointer_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
