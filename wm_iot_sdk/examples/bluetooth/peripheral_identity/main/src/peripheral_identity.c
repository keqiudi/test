/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#define LOG_TAG "identity"
#include "wm_log.h"

static struct k_work work_adv_start;
static uint8_t volatile conn_count;
static uint8_t id_current;
static bool volatile is_disconnecting;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static void adv_start(struct k_work *work)
{
    struct bt_le_adv_param adv_param = {
        .id                 = BT_ID_DEFAULT,
        .sid                = 0,
        .secondary_max_skip = 0,
        .options            = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_ONE_TIME),
        .interval_min       = 0x0020, /* 20 ms */
        .interval_max       = 0x0020, /* 20 ms */
        .peer               = NULL,
    };
    size_t id_count = 0xFF;
    int err;

    bt_id_get(NULL, &id_count);
    if (id_current == id_count) {
        int id;

        id = bt_id_create(NULL, NULL);
        if (id < 0) {
            wm_log_error("Create id failed (%d)", id);
            if (id_current == 0) {
                id_current = CONFIG_BT_MAX_CONN;
            }
            id_current--;
        } else {
            wm_log_info("New id: %d", id);
        }
    }

    wm_log_info("Using current id: %u", id_current);
    adv_param.id = id_current;

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        wm_log_error("Advertising failed to start (err %d)", err);
        return;
    }

    id_current++;
    if (id_current == CONFIG_BT_MAX_CONN) {
        id_current = 0;
    }

    wm_log_info("Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        wm_log_error("Connection failed (err 0x%02x)", err);
        return;
    }

    conn_count++;
    if (conn_count < CONFIG_BT_MAX_CONN) {
        k_work_submit(&work_adv_start);
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("Connected (%u): %s", conn_count, addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("Disconnected %s (reason 0x%02x)", addr, reason);

    if ((conn_count == 1U) && is_disconnecting) {
        is_disconnecting = false;
        k_work_submit(&work_adv_start);
    }
    conn_count--;
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("LE conn  param req: %s int (0x%04x, 0x%04x) lat %d to %d", addr, param->interval_min, param->interval_max,
                param->latency, param->timeout);

    return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("LE conn param updated: %s int 0x%04x lat %d to %d", addr, interval, latency, timeout);
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void le_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("LE PHY Updated: %s Tx 0x%x, Rx 0x%x", addr, param->tx_phy, param->rx_phy);
}
#endif /* CONFIG_BT_USER_PHY_UPDATE */

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void le_data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("Data length updated: %s max tx %u (%u us) max rx %u (%u us)", addr, info->tx_max_len, info->tx_max_time,
                info->rx_max_len, info->rx_max_time);
}
#endif /* CONFIG_BT_USER_DATA_LEN_UPDATE */

#if defined(CONFIG_BT_SMP)
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        wm_log_info("Security changed: %s level %u", addr, level);
    } else {
        wm_log_error("Security failed: %s level %u err %d", addr, level, err);
    }
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    wm_log_info("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_callbacks = {
    .cancel = auth_cancel,
};
#endif /* CONFIG_BT_SMP */

static struct bt_conn_cb conn_callbacks = {
    .connected        = connected,
    .disconnected     = disconnected,
    .le_param_req     = le_param_req,
    .le_param_updated = le_param_updated,

#if defined(CONFIG_BT_SMP)
    .security_changed = security_changed,
#endif /* CONFIG_BT_SMP */

#if defined(CONFIG_BT_USER_PHY_UPDATE)
    .le_phy_updated = le_phy_updated,
#endif /* CONFIG_BT_USER_PHY_UPDATE */

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
    .le_data_len_updated = le_data_len_updated,
#endif /* CONFIG_BT_USER_DATA_LEN_UPDATE */
};

#if defined(CONFIG_BT_OBSERVER)
#define BT_LE_SCAN_PASSIVE_ALLOW_DUPILCATES \
    BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_PASSIVE, BT_LE_SCAN_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_INTERVAL)

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    wm_log_info("Device found: %s (RSSI %d)", addr_str, rssi);
}
#endif /* CONFIG_BT_OBSERVER */

int init_peripheral(uint8_t iterations)
{
    size_t id_count;
    int err;

    err = bt_enable(NULL);
    if (err) {
        wm_log_error("Bluetooth init failed (err %d)", err);
        return err;
    }

    bt_conn_cb_register(&conn_callbacks);

#if defined(CONFIG_BT_SMP)
    bt_conn_auth_cb_register(&auth_callbacks);
#endif /* CONFIG_BT_SMP */

    wm_log_info("Bluetooth initialized");

#if defined(CONFIG_BT_OBSERVER)
    wm_log_info("Start continuous passive scanning...");
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE_ALLOW_DUPILCATES, device_found);
    if (err) {
        printk("Scan start failed (%d).", err);
        return err;
    }
    wm_log_info("success.");
#endif /* CONFIG_BT_OBSERVER */

    k_work_init(&work_adv_start, adv_start);
    k_work_submit(&work_adv_start);

    /* wait for connection attempts on all identities */
    do {
        k_sleep(K_MSEC(10));

        id_count = 0xFF;
        bt_id_get(NULL, &id_count);
    } while (id_count != CONFIG_BT_MAX_CONN);

    /* rotate identities so reconnections are attempted in case of any
	 * disconnections
	 */
    uint8_t prev_count = conn_count;
    while (1) {
        /* If maximum connections is reached, wait for disconnections
		 * initiated by peer central devices.
		 */
        if (conn_count == CONFIG_BT_MAX_CONN) {
            if (!iterations) {
                break;
            }
            iterations--;
            wm_log_info("Iterations remaining: %u", iterations);

            wm_log_info("Wait for disconnections...");
            is_disconnecting = true;
            while (is_disconnecting) {
                k_sleep(K_MSEC(10));
            }
            wm_log_info("All disconnected.");

            continue;
        }

        /* As long as there is connection count changes, identity
		 * rotation in this loop is not needed.
		 */
        if (prev_count != conn_count) {
            prev_count = conn_count;

            continue;
        } else {
            uint16_t wait = 6200U;

            /* Maximum duration without connection count change,
			 * central waiting before disconnecting all its
			 * connections plus few seconds of margin.
			 */
            while ((prev_count == conn_count) && wait) {
                wait--;

                k_sleep(K_MSEC(10));
            }

            if (wait) {
                continue;
            }
        }

        /* Stopping and restarting advertising to use new identity */
        wm_log_info("Stop advertising...");
        err = bt_le_adv_stop();
        if (err) {
            wm_log_error("Failed to stop advertising (%d)", err);

            return err;
        }

        k_work_submit(&work_adv_start);
    }

    return 0;
}
