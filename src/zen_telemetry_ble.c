/*
 * Copyright (c) 2026 zen-keyboard contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * BLE GATT transport for ZEN telemetry.
 *
 * The service rides the connection macOS already holds for HID, so the HUD does
 * not need a cable, a second pairing, or any input monitoring permission. This
 * mirrors how ZMK Studio exposes its own RPC service (app/src/studio/
 * gatt_rpc_transport.c) -- notifications instead of indications, because a HUD
 * would rather drop a frame than block the link waiting for an ack.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>

#include "zen_telemetry.h"

LOG_MODULE_DECLARE(zen_telemetry, CONFIG_ZEN_TELEMETRY_LOG_LEVEL);

/* 47c59b6d-048a-4fa7-921f-955c966a2c38 and neighbours. */
#define ZEN_TM_UUID(num) BT_UUID_128_ENCODE(num, 0x048a, 0x4fa7, 0x921f, 0x955c966a2c38)
#define ZEN_TM_SERVICE_UUID ZEN_TM_UUID(0x47c59b6d)
#define ZEN_TM_EVENTS_CHRC_UUID ZEN_TM_UUID(0x47c59b6e)
#define ZEN_TM_SNAPSHOT_CHRC_UUID ZEN_TM_UUID(0x47c59b6f)

static atomic_t events_subscribed;
static atomic_t snapshot_subscribed;

static void events_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    ARG_UNUSED(attr);

    bool enabled = (value == BT_GATT_CCC_NOTIFY);
    atomic_set(&events_subscribed, enabled ? 1 : 0);
    LOG_INF("telemetry events notifications %s", enabled ? "enabled" : "disabled");

    if (enabled) {
        /* Deferred to the telemetry work queue -- never notify from here. */
        zen_telemetry_request_snapshot();
    }
}

static void snapshot_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    ARG_UNUSED(attr);

    bool enabled = (value == BT_GATT_CCC_NOTIFY);
    atomic_set(&snapshot_subscribed, enabled ? 1 : 0);

    if (enabled) {
        zen_telemetry_request_snapshot();
    }
}

static ssize_t read_snapshot(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                             uint16_t len, uint16_t offset) {
    uint8_t snapshot[ZEN_TM_SNAPSHOT_LEN];
    zen_telemetry_fill_snapshot(snapshot);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, snapshot, sizeof(snapshot));
}

BT_GATT_SERVICE_DEFINE(
    zen_telemetry_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ZEN_TM_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZEN_TM_EVENTS_CHRC_UUID), BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(events_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZEN_TM_SNAPSHOT_CHRC_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT,
                           read_snapshot, NULL, NULL),
    BT_GATT_CCC(snapshot_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

/* attrs[1] and attrs[4] are the characteristic declarations; bt_gatt_notify()
 * walks from a declaration to its value attribute for us. */
#define ZEN_TM_EVENTS_ATTR (&zen_telemetry_svc.attrs[1])
#define ZEN_TM_SNAPSHOT_ATTR (&zen_telemetry_svc.attrs[4])

static bool ble_is_ready(void) {
    /* Kept cheap: this runs on every key event, from the ZMK listener. */
    return atomic_get(&events_subscribed) != 0;
}

static size_t ble_max_payload(void) {
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    size_t payload = ZEN_TM_MIN_PAYLOAD;

    if (conn != NULL) {
        uint16_t mtu = bt_gatt_get_mtu(conn);
        if (mtu > 3) {
            payload = mtu - 3;
        }
        bt_conn_unref(conn);
    }

    return payload;
}

static int ble_notify(const struct bt_gatt_attr *attr, const uint8_t *data, size_t len) {
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (conn == NULL) {
        return -ENOTCONN;
    }

    int err = bt_gatt_notify(conn, attr, data, len);
    bt_conn_unref(conn);

    return err;
}

static int ble_send_events(const uint8_t *data, size_t len) {
    return ble_notify(ZEN_TM_EVENTS_ATTR, data, len);
}

static int ble_send_snapshot(const uint8_t *data, size_t len) {
    if (atomic_get(&snapshot_subscribed) == 0) {
        /* The host reads the snapshot characteristic instead; nothing to push. */
        return 0;
    }

    return ble_notify(ZEN_TM_SNAPSHOT_ATTR, data, len);
}

static const struct zen_telemetry_sink ble_sink = {
    .max_payload = ble_max_payload,
    .is_ready = ble_is_ready,
    .send_events = ble_send_events,
    .send_snapshot = ble_send_snapshot,
};

static int zen_telemetry_ble_init(void) {
    zen_telemetry_register_sink(&ble_sink);
    return 0;
}

SYS_INIT(zen_telemetry_ble_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
