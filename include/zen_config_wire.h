// SPDX-License-Identifier: GPL-2.0-or-later
//
// Wire protocol for the zen_config_serial_bridge. Framing mirrors the
// pattern used elsewhere in this repo for USB-serial config bridges: a small
// fixed header (magic/version/command/payload_len) followed by a
// command-specific payload. Unlike a fire-and-forget "apply" bridge, this one
// is request/response: every host->device command gets exactly one
// device->host response frame using the same framing, with the response
// command byte equal to the request command with bit 0x80 set, and payload[0]
// holding a status code (see enum zen_config_status).
//
// This deliberately does not use ZMK Studio's own RPC/protobuf transport —
// it calls the same underlying ZMK core keymap/behavior functions Studio
// itself calls, via its own minimal framing, so a companion app doesn't need
// to speak Studio's protocol.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZEN_CONFIG_WIRE_MAGIC 0x314E455Au /* "ZEN1", little-endian */
#define ZEN_CONFIG_WIRE_VERSION 1u
#define ZEN_CONFIG_WIRE_HEADER_SIZE 8u

#define ZEN_CONFIG_WIRE_BEHAVIOR_NAME_LEN 16u

enum zen_config_wire_command {
    ZEN_CONFIG_CMD_GET_POINTER = 0x01,
    ZEN_CONFIG_CMD_SET_POINTER = 0x02,
    ZEN_CONFIG_CMD_SAVE_POINTER = 0x03,
    ZEN_CONFIG_CMD_RESET_POINTER = 0x04,
    /* Auto-mouse lives in the same settings record as the pointer settings,
     * so SAVE_POINTER and RESET_POINTER cover it too -- only the read and
     * write of the fields themselves need commands of their own. */
    ZEN_CONFIG_CMD_GET_AUTO_MOUSE = 0x05,
    ZEN_CONFIG_CMD_SET_AUTO_MOUSE = 0x06,

    ZEN_CONFIG_CMD_GET_KEYMAP_META = 0x10,
    ZEN_CONFIG_CMD_GET_LAYER = 0x11,
    ZEN_CONFIG_CMD_SET_BINDING = 0x12,
    ZEN_CONFIG_CMD_SAVE_KEYMAP = 0x13,
    ZEN_CONFIG_CMD_DISCARD_KEYMAP = 0x14,
    /* Not the same as DISCARD: that one drops unsaved edits and leaves what
     * was saved in place. This throws the saved keymap away entirely, so the
     * keyboard comes back on the one compiled into the firmware -- the way out
     * for someone who wants their old firmware's keymap back without the
     * settings-reset UF2, which takes the Bluetooth pairings with it. */
    ZEN_CONFIG_CMD_RESET_KEYMAP = 0x15,

    ZEN_CONFIG_CMD_GET_STATUS = 0x20,
    ZEN_CONFIG_CMD_GET_BLE_PROFILES = 0x21,
    ZEN_CONFIG_CMD_BLE_ACTION = 0x22,

    /* Editable combos. Only present when CONFIG_ZEN_RUNTIME_COMBO is on,
     * which shipped firmware leaves off; both answer ERR_UNKNOWN_COMMAND
     * otherwise, so a host can probe with GET_COMBO_META. */
    ZEN_CONFIG_CMD_GET_COMBO_META = 0x30,
    ZEN_CONFIG_CMD_SET_COMBO = 0x31,
    ZEN_CONFIG_CMD_GET_COMBO = 0x32,
    /* An edit takes effect at once but is only in RAM until this. Trying a
     * combo out and pulling the cable should leave nothing behind. */
    ZEN_CONFIG_CMD_SAVE_COMBO = 0x33,
    /* Empties every slot and drops the saved record, so the keyboard comes
     * back on the combos compiled into the firmware. */
    ZEN_CONFIG_CMD_RESET_COMBO = 0x34,

    /* Editable macros. Only present when CONFIG_ZEN_RUNTIME_MACRO is on;
     * probe with GET_MACRO_META the way combos are probed. */
    ZEN_CONFIG_CMD_GET_MACRO_META = 0x40,
    ZEN_CONFIG_CMD_SET_MACRO = 0x41,
    ZEN_CONFIG_CMD_GET_MACRO = 0x42,
    ZEN_CONFIG_CMD_SAVE_MACRO = 0x43,
    ZEN_CONFIG_CMD_RESET_MACRO = 0x44,
};

/* CMD_BLE_ACTION request payload: { action, profile_index }. The index is
 * ignored by CLEAR_ALL. Every action answers with the same payload
 * CMD_GET_BLE_PROFILES returns, already reflecting the change, so the host
 * never has to guess what the keyboard did. */
enum zen_config_ble_action {
    ZEN_CONFIG_BLE_ACTION_SELECT = 0,     /* make this the profile keys go to */
    ZEN_CONFIG_BLE_ACTION_DISCONNECT = 1, /* drop the link, keep the bond */
    ZEN_CONFIG_BLE_ACTION_CLEAR = 2,      /* forget one device */
    ZEN_CONFIG_BLE_ACTION_CLEAR_ALL = 3,  /* forget all of them */
};

#define ZEN_CONFIG_RESPONSE_BIT 0x80u

enum zen_config_status {
    ZEN_CONFIG_STATUS_OK = 0,
    ZEN_CONFIG_STATUS_ERR_UNKNOWN_COMMAND = 1,
    ZEN_CONFIG_STATUS_ERR_MALFORMED = 2,
    ZEN_CONFIG_STATUS_ERR_INVALID_PAYLOAD = 3,
    ZEN_CONFIG_STATUS_ERR_INVALID_LOCATION = 4,
    ZEN_CONFIG_STATUS_ERR_UNKNOWN_BEHAVIOR = 5,
    ZEN_CONFIG_STATUS_ERR_NOT_SUPPORTED = 6,
    ZEN_CONFIG_STATUS_ERR_GENERIC = 7,
    /* The combo table renumbers itself on every edit and an in-flight press
     * holds an index into it, so a write lands only with no keys down. */
    ZEN_CONFIG_STATUS_ERR_BUSY = 8,
};

/* Wire layout for CMD_GET_POINTER's response payload (after the status byte)
 * and CMD_SET_POINTER's request payload. Fixed size, explicit little-endian
 * fields (see zen_config_serial_bridge.c for encode/decode) rather than a
 * raw struct copy, so it stays stable regardless of compiler padding.
 *   [0]   settings version (see ZEN_POINTER_SETTINGS_VERSION) -- on a request
 *         this says which fields the host understands
 *   [1-2] pointer scale, percent
 *   [3-4] scroll scale, percent
 *   [5]   bit 0 invert X, bit 1 invert Y, bit 2 invert scroll
 *   [6]   trackball rotation, signed degrees (version 2 and up; a version-1
 *         host leaves this zero and the keyboard keeps its current angle)
 *   [7]   reserved, zero
 */
#define ZEN_CONFIG_WIRE_POINTER_PAYLOAD_SIZE 8u

/* Wire layout for one keymap binding entry, used in CMD_GET_LAYER's response
 * and as CMD_SET_BINDING's request payload tail. */
#define ZEN_CONFIG_WIRE_BINDING_SIZE (ZEN_CONFIG_WIRE_BEHAVIOR_NAME_LEN + 4u + 4u)

/* Wire layout for CMD_SET_COMBO's request payload. The framing validates one
 * fixed length per command, so the key-positions array is always full length
 * and only the first `count` entries are read.
 *   [0]      slot index
 *   [1]      key count; 0 clears the slot
 *   [2-5]    layer mask, le32; 0 means every layer
 *   [6-7]    timeout, le16 ms
 *   [8..]    key positions, le16 each, ZEN_CONFIG_WIRE_COMBO_MAX_KEYS of them
 *   [..]     one binding entry, as CMD_SET_BINDING carries
 *
 * GET_COMBO_META's response payload is { slots, firmware max keys, wire max
 * keys }: the firmware's own limit is CONFIG_ZEN_RUNTIME_COMBO_MAX_KEYS and
 * may be lower than the wire's, so a host has to read it rather than assume. */
#define ZEN_CONFIG_WIRE_COMBO_MAX_KEYS 6u
#define ZEN_CONFIG_WIRE_COMBO_PAYLOAD_SIZE                                                         \
    (8u + 2u * ZEN_CONFIG_WIRE_COMBO_MAX_KEYS + ZEN_CONFIG_WIRE_BINDING_SIZE)

/* CMD_GET_COMBO takes one byte, the slot, and answers with exactly the
 * payload CMD_SET_COMBO takes -- so a host can read a slot, change one field
 * and send it straight back. An empty slot answers with count 0 and the rest
 * zeroed. */
#define ZEN_CONFIG_WIRE_GET_COMBO_REQUEST_SIZE 1u

/* Wire layout for CMD_SET_MACRO's request payload and CMD_GET_MACRO's
 * response. One fixed length per command, so the step array is always full
 * and only the first `count` entries are read.
 *   [0]    slot index
 *   [1]    step count; 0 empties the slot
 *   [2-3]  reserved, zero
 *   [4..]  steps, ZEN_CONFIG_WIRE_MACRO_MAX_STEPS of them, each:
 *            [0]   op (0 tap, 1 press, 2 release, 3 wait)
 *            [1-3] reserved, zero
 *            [4-7] value, le32 -- a ZMK keycode, or milliseconds for wait
 *
 * A wait applies after the step before it; one in first place has nothing to
 * delay and is ignored. GET_MACRO_META answers { slots, firmware max steps,
 * wire max steps }, so a host reads the firmware's limit rather than assuming
 * this one. */
#define ZEN_CONFIG_WIRE_MACRO_MAX_STEPS 16u
#define ZEN_CONFIG_WIRE_MACRO_STEP_SIZE 8u
#define ZEN_CONFIG_WIRE_MACRO_PAYLOAD_SIZE                                                         \
    (4u + ZEN_CONFIG_WIRE_MACRO_MAX_STEPS * ZEN_CONFIG_WIRE_MACRO_STEP_SIZE)
#define ZEN_CONFIG_WIRE_GET_MACRO_REQUEST_SIZE 1u

/* Wire layout for CMD_GET_STATUS's response payload (after the status
 * byte). Fields that don't apply to this board/role (e.g. peripheral
 * battery on a non-split build) are 0xFF. Byte order:
 *   [0] local_battery_percent   (0-100, or 0xFF if unknown)
 *   [1] has_peripheral_battery  (0 or 1)
 *   [2] peripheral_battery_percent (0-100, valid only if [1] == 1)
 *   [3] role                    (0 = standalone, 1 = split central, 2 = split peripheral)
 *   [4] ble_connected           (0 or 1; central: active profile connected,
 *                                 peripheral: connected to its central)
 *   [5] active_profile_index    (0-4, central only, else 0xFF)
 *   [6] transport                (0 = USB, 1 = BLE, central only, else 0xFF)
 *   [7] side                     (0 = unknown, 1 = left, 2 = right)
 *
 * [7] was added later. A host that reads a 7-byte payload is talking to
 * firmware from before the LCD build existed, where the central is always the
 * right half; treat the side as unknown and assume right. It matters because
 * an LCD build inverts the halves, and the host otherwise labels the two
 * batteries the wrong way round.
 */
#define ZEN_CONFIG_WIRE_STATUS_PAYLOAD_SIZE 8u

/* Wire layout for CMD_GET_BLE_PROFILES' and CMD_BLE_ACTION's response
 * payload (after the status byte). Byte order:
 *   [0] profile_count -- how many of the entries below are real. ZMK derives
 *       this from CONFIG_BT_MAX_PAIRED minus the split peripherals, so it is
 *       a property of the firmware, not a constant the host can assume.
 *   [1] active_profile_index (0xFF if none)
 *   [2 + i] flags for profile i, zero past profile_count:
 *       bit 0  bonded    -- a device is registered in this slot
 *       bit 1  connected -- that device is connected right now
 * Both handlers answer ERR_NOT_SUPPORTED on a split peripheral, which has no
 * profiles of its own.
 */
/* Wire layout for CMD_GET_AUTO_MOUSE's response payload (after the status
 * byte) and CMD_SET_AUTO_MOUSE's request payload. Byte order:
 *   [0]   settings version (see ZEN_POINTER_SETTINGS_VERSION)
 *   [1]   enabled (0 or 1)
 *   [2-3] how long the layer stays on after the ball stops, ms
 *   [4-5] refuse to switch layers this soon after a keystroke, ms
 */
#define ZEN_CONFIG_WIRE_AUTO_MOUSE_PAYLOAD_SIZE 6u

#define ZEN_CONFIG_WIRE_BLE_PROFILES_MAX 8u
#define ZEN_CONFIG_WIRE_BLE_PROFILES_PAYLOAD_SIZE (2u + ZEN_CONFIG_WIRE_BLE_PROFILES_MAX)

/* Spelled out rather than BIT(), since this header only pulls in stdint.h. */
#define ZEN_CONFIG_WIRE_BLE_PROFILE_BONDED 0x01u
#define ZEN_CONFIG_WIRE_BLE_PROFILE_CONNECTED 0x02u

#ifdef __cplusplus
}
#endif
