/*
 * Copyright (c) 2026 zen-keyboard contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Host names for the `profiles` characteristic.
 *
 * ZMK remembers only an address per BLE profile, which tells a person nothing
 * about which machine that is. Every host publishes its own name in the GAP
 * Device Name characteristic -- the computer name on Windows, "<user>'s Mac
 * mini" on macOS -- so once a host is connected and encrypted this reads that
 * name once and keeps it, paired with the address it came from.
 *
 * Kept deliberately small and gentle: one read per connection, a few seconds
 * after the link is up so it never competes with the host's own HID setup, one
 * read in flight at a time, and a flash write only when a name changes.
 *
 * Both the read and the flash write run on the telemetry queue, never the
 * system workqueue. A GATT request can wait up to BT_ATT_TIMEOUT (30 s) for a
 * free request slot, and ZMK does its own work on the system queue; on ours,
 * the worst a stall can hold up is telemetry.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zmk/ble.h>

#include "zen_telemetry.h"

LOG_MODULE_DECLARE(zen_telemetry, CONFIG_ZEN_TELEMETRY_LOG_LEVEL);

/* Let the host finish discovering and subscribing to HID first. */
#define HOST_NAME_READ_DELAY K_SECONDS(3)

struct host_name {
    bt_addr_le_t addr;
    uint8_t len;
    uint8_t name[ZEN_TM_HOST_NAME_MAX];
};

/* Written from the BT RX thread (read callback) and at boot (settings load);
 * read from the BT RX thread (profiles read). No lock needed beyond that. */
static struct host_name host_names[ZMK_BLE_PROFILE_COUNT];

/* Profiles whose name has been read on the current connection. */
static atomic_t fetched;
/* Profiles whose stored name changed and needs saving. */
static atomic_t dirty;

/* Static on purpose: the read is asynchronous, and BT_UUID_GAP_DEVICE_NAME is a
 * compound literal that would not outlive the function that named it. */
static const struct bt_uuid_16 device_name_uuid = BT_UUID_INIT_16(BT_UUID_GAP_DEVICE_NAME_VAL);
static struct bt_gatt_read_params name_read;
static uint8_t reading_index;
static bool read_in_flight;

static void fetch_work_handler(struct k_work *work);
static void save_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(fetch_work, fetch_work_handler);
static K_WORK_DEFINE(save_work, save_work_handler);

size_t zen_telemetry_host_name(uint8_t index, const bt_addr_le_t *peer, uint8_t *out, size_t max) {
    if (index >= ZMK_BLE_PROFILE_COUNT || peer == NULL) {
        return 0;
    }

    const struct host_name *host = &host_names[index];
    if (host->len == 0 || bt_addr_le_cmp(&host->addr, peer) != 0) {
        return 0;
    }

    size_t len = MIN(host->len, max);
    memcpy(out, host->name, len);
    return len;
}

/* Cut at a UTF-8 character boundary: a Japanese name truncated mid-character
 * would otherwise end in garbage. */
static size_t utf8_trim(const uint8_t *s, size_t len) {
    size_t i = len;
    size_t continuation = 0;

    while (i > 0 && (s[i - 1] & 0xC0) == 0x80) {
        i--;
        continuation++;
    }
    if (i == 0) {
        return 0;
    }

    uint8_t lead = s[i - 1];
    size_t needed = (lead & 0x80) == 0x00   ? 0
                    : (lead & 0xE0) == 0xC0 ? 1
                    : (lead & 0xF0) == 0xE0 ? 2
                    : (lead & 0xF8) == 0xF0 ? 3
                                            : 0;

    if (continuation == needed) {
        return len;
    }
    /* Incomplete (or stray) trailing bytes: drop the partial character. */
    return needed > 0 ? i - 1 : i;
}

static void finish_read(void) {
    read_in_flight = false;
    /* Another host may be waiting its turn. */
    zen_telemetry_schedule(&fetch_work, K_NO_WAIT);
}

static uint8_t name_read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
                            const void *data, uint16_t length) {
    uint8_t index = reading_index;

    if (err != 0 || data == NULL) {
        if (err != 0) {
            LOG_DBG("host name read on profile %d failed (0x%02x)", index, err);
        }
        /* Either way this connection has had its one try. */
        atomic_set_bit(&fetched, index);
        finish_read();
        return BT_GATT_ITER_STOP;
    }

    struct host_name *host = &host_names[index];
    const bt_addr_le_t *peer = bt_conn_get_dst(conn);
    size_t len = utf8_trim(data, MIN(length, ZEN_TM_HOST_NAME_MAX));

    bool changed = host->len != len || memcmp(host->name, data, len) != 0 ||
                   bt_addr_le_cmp(&host->addr, peer) != 0;
    if (changed) {
        bt_addr_le_copy(&host->addr, peer);
        host->len = (uint8_t)len;
        memcpy(host->name, data, len);
        atomic_set_bit(&dirty, index);
        zen_telemetry_submit(&save_work);
        /* Let a subscribed companion app pick the new name up. */
        zen_telemetry_request_snapshot();
    }

    atomic_set_bit(&fetched, index);
    finish_read();
    /* The first Device Name is the one; do not walk the rest of the table. */
    return BT_GATT_ITER_STOP;
}

/* Finds one connected, encrypted host whose name has not been read on this
 * connection and reads it. Runs again when that read completes. */
static void fetch_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (read_in_flight) {
        return;
    }

    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        if (atomic_test_bit(&fetched, i) || zmk_ble_profile_is_open(i)) {
            continue;
        }

        struct bt_conn *conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, zmk_ble_profile_address(i));
        if (conn == NULL) {
            continue;
        }

        struct bt_conn_info info;
        bool ready = bt_conn_get_info(conn, &info) == 0 && info.state == BT_CONN_STATE_CONNECTED &&
                     bt_conn_get_security(conn) >= BT_SECURITY_L2;
        if (!ready) {
            bt_conn_unref(conn);
            continue;
        }

        name_read = (struct bt_gatt_read_params){
            .func = name_read_cb,
            .handle_count = 0,
            .by_uuid =
                {
                    .start_handle = 0x0001,
                    .end_handle = 0xffff,
                    .uuid = &device_name_uuid.uuid,
                },
        };
        reading_index = i;
        read_in_flight = true;

        int err = bt_gatt_read(conn, &name_read);
        bt_conn_unref(conn);

        if (err < 0) {
            LOG_DBG("could not start host name read on profile %d (%d)", i, err);
            read_in_flight = false;
            atomic_set_bit(&fetched, i);
            continue;
        }
        return;
    }
}

static void save_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        if (!atomic_test_and_clear_bit(&dirty, i)) {
            continue;
        }

        char key[24];
        snprintf(key, sizeof(key), "zen_tm/host/%d", i);
        int err = settings_save_one(key, &host_names[i], sizeof(host_names[i]));
        if (err < 0) {
            LOG_WRN("could not save host name for profile %d (%d)", i, err);
        }
    }
}

static int host_names_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    /* `name` arrives without the "zen_tm/" prefix: "host/<index>". */
    const char *next;
    if (!settings_name_steq(name, "host", &next) || next == NULL) {
        return -ENOENT;
    }

    int index = atoi(next);
    if (index < 0 || index >= ZMK_BLE_PROFILE_COUNT || len != sizeof(struct host_name)) {
        /* A profile that no longer exists, or a layout from another build. */
        return 0;
    }

    struct host_name loaded;
    if (read_cb(cb_arg, &loaded, sizeof(loaded)) != sizeof(loaded) ||
        loaded.len > ZEN_TM_HOST_NAME_MAX) {
        return 0;
    }

    host_names[index] = loaded;
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zen_tm_hosts, "zen_tm", NULL, host_names_set, NULL, NULL);

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
    ARG_UNUSED(conn);

    if (err == BT_SECURITY_ERR_SUCCESS && level >= BT_SECURITY_L2) {
        zen_telemetry_schedule(&fetch_work, HOST_NAME_READ_DELAY);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    ARG_UNUSED(reason);

    /* Read again next time: the host may have been renamed meanwhile. */
    int index = zmk_ble_profile_index(bt_conn_get_dst(conn));
    if (index >= 0 && index < ZMK_BLE_PROFILE_COUNT) {
        atomic_clear_bit(&fetched, index);
    }
}

BT_CONN_CB_DEFINE(zen_tm_hosts_conn_cb) = {
    .security_changed = security_changed,
    .disconnected = disconnected,
};
