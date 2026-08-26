/*
 * Copyright (c) 2026 zen-keyboard contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Telemetry core: subscribes to ZMK events, aggregates keyboard state and hands
 * compact frames to whichever transport registered itself as the sink.
 *
 * Listeners only copy into a ring buffer -- all transmission happens on a
 * dedicated low priority work queue so telemetry can never delay HID.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

#include <string.h>

#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
#include <zmk/events/split_peripheral_status_changed.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#endif

#include "zen_telemetry.h"

LOG_MODULE_REGISTER(zen_telemetry, CONFIG_ZEN_TELEMETRY_LOG_LEVEL);

/* ------------------------------------------------------------------ state */

static const struct zen_telemetry_sink *sink;
static bool started;

RING_BUF_DECLARE(zen_tm_ring, CONFIG_ZEN_TELEMETRY_RING_SIZE);
static struct k_spinlock zen_tm_lock;

static uint8_t pressed_bits[ZEN_TM_POS_BITS_LEN];
static uint8_t mods_mask;
static uint8_t peripheral_soc;
static bool peripheral_connected;
static uint16_t dropped;
static uint8_t frame_seq;

static atomic_t snapshot_pending;

static K_THREAD_STACK_DEFINE(zen_tm_stack, CONFIG_ZEN_TELEMETRY_STACK_SIZE);
static struct k_work_q zen_tm_workq;

static void zen_tm_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(zen_tm_work, zen_tm_work_handler);

/* -------------------------------------------------------------- ring buffer */

static size_t rec_len(uint8_t type) {
    switch (type) {
    case ZEN_TM_REC_POSITION:
        return ZEN_TM_POSITION_LEN;
    case ZEN_TM_REC_LAYER:
        return ZEN_TM_LAYER_LEN;
    case ZEN_TM_REC_KEYCODE:
        return ZEN_TM_KEYCODE_LEN;
    case ZEN_TM_REC_MODS:
        return ZEN_TM_MODS_LEN;
    default:
        return 0;
    }
}

static void submit_work(void) {
    if (started) {
        k_work_schedule_for_queue(&zen_tm_workq, &zen_tm_work, K_NO_WAIT);
    }
}

/* Push one record, evicting whole records from the front when full. A HUD is
 * better served by recent events plus a resync than by a stalled backlog. */
static void push_record(const uint8_t *rec, size_t len) {
    const struct zen_telemetry_sink *s = sink;
    if (s == NULL || !s->is_ready()) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&zen_tm_lock);

    while (ring_buf_space_get(&zen_tm_ring) < len) {
        uint8_t head;
        if (ring_buf_peek(&zen_tm_ring, &head, 1) != 1) {
            break;
        }

        size_t evict = rec_len(head);
        if (evict == 0) {
            /* Desynchronised: the only safe move is to start over. */
            ring_buf_reset(&zen_tm_ring);
            break;
        }

        ring_buf_get(&zen_tm_ring, NULL, evict);
        if (dropped < UINT16_MAX) {
            dropped++;
        }
        atomic_set(&snapshot_pending, 1);
    }

    ring_buf_put(&zen_tm_ring, rec, len);

    k_spin_unlock(&zen_tm_lock, key);

    submit_work();
}

/* ---------------------------------------------------------------- snapshot */

void zen_telemetry_fill_snapshot(uint8_t *out) {
    memset(out, 0, ZEN_TM_SNAPSHOT_LEN);

    out[0] = ZEN_TM_PROTO_VER;
    out[1] = peripheral_connected ? ZEN_TM_SNAP_FLAG_PERIPHERAL_CONNECTED : 0;
    sys_put_le32((uint32_t)zmk_keymap_layer_state(), &out[2]);

    k_spinlock_key_t key = k_spin_lock(&zen_tm_lock);
    memcpy(&out[6], pressed_bits, ZEN_TM_POS_BITS_LEN);
    sys_put_le16(dropped, &out[18]);
    k_spin_unlock(&zen_tm_lock, key);

    out[13] = mods_mask;

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    out[14] = zmk_battery_state_of_charge();
#endif
    out[15] = peripheral_soc;

    struct zmk_endpoint_instance ep = zmk_endpoints_selected();
    uint8_t endpoint = (ep.transport == ZMK_TRANSPORT_BLE) ? ZEN_TM_ENDPOINT_BLE
                                                           : ZEN_TM_ENDPOINT_USB;
#if IS_ENABLED(CONFIG_ZMK_BLE)
    int profile = zmk_ble_active_profile_index();
    if (profile >= 0) {
        endpoint |= (uint8_t)((profile & 0x0F) << 4);
    }
#endif
    out[16] = endpoint;
    out[17] = (uint8_t)zmk_keymap_highest_layer_active();
}

void zen_telemetry_request_snapshot(void) {
    atomic_set(&snapshot_pending, 1);
    submit_work();
}

/* ------------------------------------------------------------------- sender */

static void zen_tm_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    const struct zen_telemetry_sink *s = sink;
    if (s == NULL) {
        return;
    }

    if (!s->is_ready()) {
        k_spinlock_key_t key = k_spin_lock(&zen_tm_lock);
        ring_buf_reset(&zen_tm_ring);
        k_spin_unlock(&zen_tm_lock, key);
        return;
    }

    if (atomic_cas(&snapshot_pending, 1, 0)) {
        uint8_t snapshot[ZEN_TM_SNAPSHOT_LEN];
        zen_telemetry_fill_snapshot(snapshot);

        int err = s->send_snapshot(snapshot, sizeof(snapshot));
        if (err < 0) {
            LOG_DBG("snapshot send failed (%d), retrying", err);
            atomic_set(&snapshot_pending, 1);
            k_work_reschedule_for_queue(&zen_tm_workq, &zen_tm_work,
                                        K_MSEC(CONFIG_ZEN_TELEMETRY_RETRY_MS));
            return;
        }
    }

    size_t max_payload = s->max_payload();
    max_payload = CLAMP(max_payload, ZEN_TM_MIN_PAYLOAD, ZEN_TM_MAX_PAYLOAD);

    uint8_t frame[ZEN_TM_MAX_PAYLOAD];

    while (true) {
        size_t used = ZEN_TM_FRAME_HDR_LEN;

        k_spinlock_key_t key = k_spin_lock(&zen_tm_lock);
        while (true) {
            uint8_t head;
            if (ring_buf_peek(&zen_tm_ring, &head, 1) != 1) {
                break;
            }

            size_t len = rec_len(head);
            if (len == 0) {
                ring_buf_reset(&zen_tm_ring);
                break;
            }

            if (used + len > max_payload) {
                break;
            }

            ring_buf_get(&zen_tm_ring, &frame[used], len);
            used += len;
        }
        k_spin_unlock(&zen_tm_lock, key);

        if (used == ZEN_TM_FRAME_HDR_LEN) {
            return;
        }

        frame[0] = ZEN_TM_PROTO_VER;
        frame[1] = frame_seq++;

        int err = s->send_events(frame, used);
        if (err < 0) {
            /* The records in this frame are gone. The host notices the seq gap,
             * and the snapshot we queue here restores the authoritative state. */
            LOG_DBG("events send failed (%d)", err);
            atomic_set(&snapshot_pending, 1);
            k_work_reschedule_for_queue(&zen_tm_workq, &zen_tm_work,
                                        K_MSEC(CONFIG_ZEN_TELEMETRY_RETRY_MS));
            return;
        }
    }
}

/* ---------------------------------------------------------------- listeners */

static void emit_layer_state(void) {
    uint8_t rec[ZEN_TM_LAYER_LEN] = {ZEN_TM_REC_LAYER};
    sys_put_le32((uint32_t)zmk_keymap_layer_state(), &rec[1]);
    push_record(rec, sizeof(rec));
}

static void emit_mods(void) {
    uint8_t rec[ZEN_TM_MODS_LEN] = {ZEN_TM_REC_MODS, mods_mask};
    push_record(rec, sizeof(rec));
}

static void handle_position(const struct zmk_position_state_changed *ev) {
    uint32_t position = ev->position;

    if (position < ZEN_TM_POS_MAX) {
        k_spinlock_key_t key = k_spin_lock(&zen_tm_lock);
        if (ev->state) {
            pressed_bits[position / 8] |= BIT(position % 8);
        } else {
            pressed_bits[position / 8] &= ~BIT(position % 8);
        }
        k_spin_unlock(&zen_tm_lock, key);
    }

    uint8_t flags = ev->state ? ZEN_TM_POS_FLAG_PRESSED : 0;
    if (ev->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
        flags |= ZEN_TM_POS_FLAG_LEFT;
    }

    uint8_t rec[ZEN_TM_POSITION_LEN] = {ZEN_TM_REC_POSITION, flags, (uint8_t)position};
    sys_put_le16((uint16_t)(ev->timestamp & 0xFFFF), &rec[3]);
    push_record(rec, sizeof(rec));
}

static void handle_keycode(const struct zmk_keycode_state_changed *ev) {
    /* Track modifiers ourselves. This listener runs before ZMK hid_listener, so
     * zmk_hid_get_explicit_mods() would still hold the pre-event value here. */
    uint8_t bits = ev->explicit_modifiers;
    if (is_mod(ev->usage_page, ev->keycode)) {
        bits |= (uint8_t)BIT(ev->keycode - HID_USAGE_KEY_KEYBOARD_LEFTCONTROL);
    }

    if (bits != 0) {
        uint8_t before = mods_mask;
        if (ev->state) {
            mods_mask |= bits;
        } else {
            mods_mask &= ~bits;
        }
        if (mods_mask != before) {
            emit_mods();
        }
    }

    uint8_t rec[ZEN_TM_KEYCODE_LEN] = {
        ZEN_TM_REC_KEYCODE,
        ev->state ? ZEN_TM_KC_FLAG_PRESSED : 0,
        (uint8_t)(ev->usage_page & 0xFF),
    };
    sys_put_le16((uint16_t)(ev->keycode & 0xFFFF), &rec[3]);
    rec[5] = ev->implicit_modifiers;
    sys_put_le16((uint16_t)(ev->timestamp & 0xFFFF), &rec[6]);
    push_record(rec, sizeof(rec));
}

static int zen_tm_event_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(eh);
    if (pos != NULL) {
        handle_position(pos);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_layer_state_changed(eh) != NULL) {
        emit_layer_state();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_keycode_state_changed *kc = as_zmk_keycode_state_changed(eh);
    if (kc != NULL) {
        handle_keycode(kc);
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_modifiers_state_changed *mods = as_zmk_modifiers_state_changed(eh);
    if (mods != NULL) {
        uint8_t before = mods_mask;
        if (mods->state) {
            mods_mask |= mods->modifiers;
        } else {
            mods_mask &= ~mods->modifiers;
        }
        if (mods_mask != before) {
            emit_mods();
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_endpoint_changed(eh) != NULL) {
        zen_telemetry_request_snapshot();
        return ZMK_EV_EVENT_BUBBLE;
    }

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    if (as_zmk_battery_state_changed(eh) != NULL) {
        zen_telemetry_request_snapshot();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_peripheral_battery_state_changed *pb =
        as_zmk_peripheral_battery_state_changed(eh);
    if (pb != NULL) {
        peripheral_soc = pb->state_of_charge;
        zen_telemetry_request_snapshot();
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    const struct zmk_split_peripheral_status_changed *sp =
        as_zmk_split_peripheral_status_changed(eh);
    if (sp != NULL) {
        peripheral_connected = sp->connected;
        zen_telemetry_request_snapshot();
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zen_telemetry, zen_tm_event_listener);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_position_state_changed);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_modifiers_state_changed);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
ZMK_SUBSCRIPTION(zen_telemetry, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(zen_telemetry, zmk_peripheral_battery_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
ZMK_SUBSCRIPTION(zen_telemetry, zmk_split_peripheral_status_changed);
#endif

/* --------------------------------------------------------------------- init */

void zen_telemetry_register_sink(const struct zen_telemetry_sink *new_sink) {
    sink = new_sink;
}

static int zen_telemetry_init(void) {
    k_work_queue_start(&zen_tm_workq, zen_tm_stack, K_THREAD_STACK_SIZEOF(zen_tm_stack),
                       CONFIG_ZEN_TELEMETRY_THREAD_PRIORITY, NULL);
    k_thread_name_set(&zen_tm_workq.thread, "zen_tm");
    started = true;

    /* A sink may already have subscribed while we were still initialising. */
    if (atomic_get(&snapshot_pending) != 0) {
        submit_work();
    }

    LOG_INF("ZEN telemetry ready (proto v%d)", ZEN_TM_PROTO_VER);
    return 0;
}

SYS_INIT(zen_telemetry_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
