/*
 * Copyright (c) 2026 zen-keyboard contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * ZEN telemetry: streams live keyboard state (layers, key positions, keycodes,
 * modifiers, device status) to a companion app such as zen-hud.
 *
 * The wire format is documented in docs/zen-telemetry-protocol.md. Keep the two
 * in sync -- the macOS decoder is written against that document, not this header.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/addr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZEN_TM_PROTO_VER 1

/* An events frame is a 2 byte header followed by back-to-back records. */
#define ZEN_TM_FRAME_HDR_LEN 2

/* Worst case payload a transport must accept: ATT MTU 23 minus the 3 byte
 * notification header. Anything larger is a bonus we use when offered. */
#define ZEN_TM_MIN_PAYLOAD 20
#define ZEN_TM_MAX_PAYLOAD 64

/* Record types. The type byte alone determines the record length, so a reader
 * never needs a length field. */
#define ZEN_TM_REC_POSITION 0x01
#define ZEN_TM_REC_LAYER 0x02
#define ZEN_TM_REC_KEYCODE 0x03
#define ZEN_TM_REC_MODS 0x04

#define ZEN_TM_POSITION_LEN 5
#define ZEN_TM_LAYER_LEN 5
#define ZEN_TM_KEYCODE_LEN 8
#define ZEN_TM_MODS_LEN 2
#define ZEN_TM_REC_MAX_LEN 8

/* ZEN_TM_REC_POSITION flags */
#define ZEN_TM_POS_FLAG_PRESSED BIT(0)
#define ZEN_TM_POS_FLAG_LEFT BIT(1)

/* ZEN_TM_REC_KEYCODE flags */
#define ZEN_TM_KC_FLAG_PRESSED BIT(0)

/* Snapshot layout */
#define ZEN_TM_SNAPSHOT_LEN 20
#define ZEN_TM_POS_BITS_LEN 7
#define ZEN_TM_POS_MAX (ZEN_TM_POS_BITS_LEN * 8)

/* Snapshot flags byte */
#define ZEN_TM_SNAP_FLAG_PERIPHERAL_CONNECTED BIT(0)

/* Snapshot endpoint byte: low nibble transport, high nibble BLE profile index. */
#define ZEN_TM_ENDPOINT_USB 0x00
#define ZEN_TM_ENDPOINT_BLE 0x01

/* `profiles` characteristic: which host each BLE profile is bonded to.
 * A 2 byte header (version, slot count) followed by one slot each: a 9 byte
 * fixed part (flags, address type, address, name length) and then the name. */
#define ZEN_TM_PROFILES_VER 2
#define ZEN_TM_PROFILES_HDR_LEN 2
#define ZEN_TM_PROFILE_SLOT_FIXED_LEN 9

/* Longest host name kept, in bytes of UTF-8. Longer names are cut at a
 * character boundary. */
#define ZEN_TM_HOST_NAME_MAX 32

/* Profile slot flags byte */
#define ZEN_TM_PROFILE_FLAG_OPEN BIT(0)
#define ZEN_TM_PROFILE_FLAG_CONNECTED BIT(1)
#define ZEN_TM_PROFILE_FLAG_ACTIVE BIT(2)

/**
 * @brief A transport that carries telemetry to the host.
 *
 * Exactly one sink is active at a time. BLE GATT is the shipped one; the same
 * interface is what a USB CDC fallback would implement.
 */
struct zen_telemetry_sink {
    /** Largest payload one frame may carry. Clamped to [MIN, MAX] by the core. */
    size_t (*max_payload)(void);
    /** True while a host is subscribed. Telemetry is discarded when false. */
    bool (*is_ready)(void);
    /**
     * True when the host being typed on can take events. Snapshots still go
     * out when false; buffered events are discarded without spending a frame
     * sequence number, so a host that comes back sees no false gap. Optional:
     * NULL means always.
     */
    bool (*events_deliverable)(void);
    int (*send_events)(const uint8_t *data, size_t len);
    int (*send_snapshot)(const uint8_t *data, size_t len);
};

/** Install the transport. Called from the sink SYS_INIT hook. */
void zen_telemetry_register_sink(const struct zen_telemetry_sink *sink);

/** Queue a full-state snapshot. Sinks call this when a host subscribes. */
void zen_telemetry_request_snapshot(void);

/**
 * Run work on the telemetry queue: a low priority thread of its own, never the
 * system workqueue ZMK depends on. Anything telemetry does that can block -- a
 * GATT request waiting for a buffer, a flash write -- goes here, so the worst
 * it can stall is telemetry. Dropped silently before the queue has started.
 */
void zen_telemetry_schedule(struct k_work_delayable *work, k_timeout_t delay);
void zen_telemetry_submit(struct k_work *work);

/** Write ZEN_TM_SNAPSHOT_LEN bytes of current state. Safe from any thread. */
void zen_telemetry_fill_snapshot(uint8_t *out);

/**
 * Copy the GAP Device Name last read from the host bonded to profile `index`
 * into `out` (not NUL terminated) and return its length. Returns 0 when no
 * name is known, or when the name was read from a different host than `peer`
 * -- the profile has been cleared or re-paired since.
 */
size_t zen_telemetry_host_name(uint8_t index, const bt_addr_le_t *peer, uint8_t *out, size_t max);
