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
    int (*send_events)(const uint8_t *data, size_t len);
    int (*send_snapshot)(const uint8_t *data, size_t len);
};

/** Install the transport. Called from the sink SYS_INIT hook. */
void zen_telemetry_register_sink(const struct zen_telemetry_sink *sink);

/** Queue a full-state snapshot. Sinks call this when a host subscribes. */
void zen_telemetry_request_snapshot(void);

/** Write ZEN_TM_SNAPSHOT_LEN bytes of current state. Safe from any thread. */
void zen_telemetry_fill_snapshot(uint8_t *out);
