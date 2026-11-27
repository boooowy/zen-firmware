// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 2 added rotation_degrees, 3 added the auto-mouse fields. Records written by
 * any earlier version are still readable -- see zen_pointer_settings_set() --
 * so an existing keyboard keeps its settings across an upgrade instead of
 * silently reverting to the defaults. */
#define ZEN_POINTER_SETTINGS_VERSION 3
#define ZEN_POINTER_SETTINGS_SCALE_MIN 20
#define ZEN_POINTER_SETTINGS_SCALE_MAX 300
#define ZEN_POINTER_SETTINGS_SCALE_DEFAULT 100
/* Enough to square up a keyboard placed at an angle; past this you are better
 * off moving the keyboard. The wire format carries this as a signed byte. */
#define ZEN_POINTER_SETTINGS_ROTATION_MIN (-45)
#define ZEN_POINTER_SETTINGS_ROTATION_MAX 45
#define ZEN_POINTER_SETTINGS_ROTATION_DEFAULT 0

/* Auto-mouse: hold the pointing layer while the ball is moving.
 *
 * The defaults reproduce what the devicetree used to hardcode
 * (zip_temp_layer 1 500, with no require-prior-idle-ms), so updating the
 * firmware does not change how anyone's keyboard already behaves. */
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_ENABLED_DEFAULT true
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_MIN 100
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_MAX 2000
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_TIMEOUT_DEFAULT 500
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_MIN 0
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_MAX 500
#define ZEN_POINTER_SETTINGS_AUTO_MOUSE_IDLE_DEFAULT 0

struct zen_pointer_settings_payload {
    uint8_t version;
    uint16_t pointer_scale_percent;
    uint16_t scroll_scale_percent;
    bool invert_x;
    bool invert_y;
    bool invert_scroll;
    int8_t rotation_degrees;
    bool auto_mouse_enabled;
    /* How long the pointing layer stays on after the ball stops. */
    uint16_t auto_mouse_timeout_ms;
    /* Refuse to switch layers this soon after a keystroke, so a hand brushing
     * the ball mid-word does not swap the layer out from under you. */
    uint16_t auto_mouse_idle_ms;
};

const struct zen_pointer_settings_payload *zen_pointer_settings_payload(void);
/* zen_pointer_settings_is_valid_payload() lives in zen_config_codec.h, with
 * the encode/decode it has to agree with. */
bool zen_pointer_settings_loaded(void);
int zen_pointer_settings_reset_to_defaults(void);
int zen_pointer_settings_apply_payload(const struct zen_pointer_settings_payload *payload, bool persist);
int zen_pointer_settings_copy_payload(struct zen_pointer_settings_payload *payload_out);

#ifdef __cplusplus
}
#endif
