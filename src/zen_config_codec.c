// SPDX-License-Identifier: GPL-2.0-or-later

#include "zen_config_codec.h"

#include <errno.h>

#include "zen_config_wire.h"

/* Spelled out rather than using Zephyr's byteorder helpers: keeping this file
 * free of Zephyr is the whole point, so it can be compiled and tested on the
 * host. Little-endian on the wire, matching the rest of the protocol. */
static uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }

static void put_le16(uint16_t value, uint8_t *p) {
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)(value >> 8);
}

bool zen_pointer_settings_is_valid_payload(const struct zen_pointer_settings_payload *payload) {
    if (!payload) {
        return false;
    }

    if (payload->version != ZEN_POINTER_SETTINGS_VERSION) {
        return false;
    }

    if (payload->pointer_scale_percent < ZEN_POINTER_SETTINGS_SCALE_MIN ||
        payload->pointer_scale_percent > ZEN_POINTER_SETTINGS_SCALE_MAX) {
        return false;
    }

    if (payload->scroll_scale_percent < ZEN_POINTER_SETTINGS_SCALE_MIN ||
        payload->scroll_scale_percent > ZEN_POINTER_SETTINGS_SCALE_MAX) {
        return false;
    }

    if (payload->rotation_degrees < ZEN_POINTER_SETTINGS_ROTATION_MIN ||
        payload->rotation_degrees > ZEN_POINTER_SETTINGS_ROTATION_MAX) {
        return false;
    }

    if (payload->auto_mouse_timeout_ms < ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_MIN ||
        payload->auto_mouse_timeout_ms > ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_MAX) {
        return false;
    }

    if (payload->auto_mouse_idle_ms > ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_MAX) {
        return false;
    }

    return true;
}

void zen_config_encode_pointer(const struct zen_pointer_settings_payload *in, uint8_t *out) {
    out[0] = in->version;
    put_le16(in->pointer_scale_percent, &out[1]);
    put_le16(in->scroll_scale_percent, &out[3]);
    out[5] = (uint8_t)((in->invert_x ? 0x01 : 0) | (in->invert_y ? 0x02 : 0) |
                       (in->invert_scroll ? 0x04 : 0));
    out[6] = (uint8_t)in->rotation_degrees;
    out[7] = 0;
}

int zen_config_decode_pointer(const uint8_t *in, struct zen_pointer_settings_payload *record) {
    record->pointer_scale_percent = get_le16(&in[1]);
    record->scroll_scale_percent = get_le16(&in[3]);
    record->invert_x = (in[5] & 0x01) != 0;
    record->invert_y = (in[5] & 0x02) != 0;
    record->invert_scroll = (in[5] & 0x04) != 0;

    /* Byte 6 was a reserved zero before rotation existed, so a host that
     * predates it would otherwise read as "straighten the trackball". Leaving
     * it alone keeps whatever the keyboard already has. */
    if (in[0] >= 2) {
        record->rotation_degrees = (int8_t)in[6];
    }

    /* The host only says which fields it understands; the record this becomes
     * is always in our own current format. */
    record->version = ZEN_POINTER_SETTINGS_VERSION;

    return zen_pointer_settings_is_valid_payload(record) ? 0 : -EINVAL;
}

void zen_config_encode_auto_mouse(const struct zen_pointer_settings_payload *in, uint8_t *out) {
    out[0] = in->version;
    out[1] = in->auto_mouse_enabled ? 1 : 0;
    put_le16(in->auto_mouse_timeout_ms, &out[2]);
    put_le16(in->auto_mouse_idle_ms, &out[4]);
}

int zen_config_decode_auto_mouse(const uint8_t *in, struct zen_pointer_settings_payload *record) {
    record->auto_mouse_enabled = in[1] != 0;
    record->auto_mouse_timeout_ms = get_le16(&in[2]);
    record->auto_mouse_idle_ms = get_le16(&in[4]);
    record->version = ZEN_POINTER_SETTINGS_VERSION;

    return zen_pointer_settings_is_valid_payload(record) ? 0 : -EINVAL;
}
