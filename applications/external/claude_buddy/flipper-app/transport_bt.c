/**
 * Bluetooth BLE serial transport.
 *
 * Uses the Flipper SDK's ble_profile_serial API to expose a BLE serial
 * service. The host bridge connects via bleak and communicates over GATT.
 *
 * Flow:
 *   1. Open the Bt service record
 *   2. Start the serial profile (replaces default RPC profile)
 *   3. Set RX callback and start advertising
 *   4. On exit, restore the default profile
 */

#include "transport.h"
#include "protocol.h"
#include <furi.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>

#define TAG "BtTransport"
#define BT_SERIAL_BUFFER_SIZE 512
#define BT_TX_CHUNK 240  /* stay under BLE_SVC_SERIAL_CHAR_VALUE_LEN_MAX (243) */

typedef struct {
    Transport            base;   /* MUST be first */
    Bt*                  bt;
    FuriHalBleProfileBase* profile;
    TransportRxCallback  callback;
    void*                callback_ctx;
    TransportConnectCallback connect_cb;
    void*                connect_ctx;
    bool                 connected;
    bool                 hello_sent; /* track per-connection hello */
    /* Line-buffered RX */
    char                 rx_buf[PROTOCOL_MAX_MSG_LEN];
    int                  rx_pos;
} BtTransport;

/* ── BLE event callbacks (BLE GAP thread — NOT GUI thread) ──── */

static uint16_t bt_serial_event_cb(SerialServiceEvent event, void* context) {
    BtTransport* bt = context;
    if(!bt) return 0;

    if(event.event == SerialServiceEventTypeDataReceived) {
        const uint8_t* data = event.data.buffer;
        uint16_t len = event.data.size;
        if(!data && len > 0) return 0;

        FURI_LOG_D(TAG, "RX chunk: %u bytes", len);

        /* Line-buffer incoming data, dispatch complete lines.
         *
         * IMPORTANT: Do NOT call ble_profile_serial_tx() from inside this
         * callback — the BLE stack holds a mutex when calling us, and
         * ble_profile_serial_tx() needs the same mutex, causing a deadlock
         * on Momentum firmware.  All TX (hello, pong, etc.) goes through
         * the GUI thread via the app callback → message queue path. */
        for(uint16_t i = 0; i < len; i++) {
            if(data[i] == '\n') {
                if(bt->rx_pos > 0) {
                    bt->rx_buf[bt->rx_pos] = '\0';

                    FURI_LOG_D(TAG, "RX line: %s", bt->rx_buf);

                    if(bt->callback)
                        bt->callback(bt->rx_buf, bt->callback_ctx);
                    bt->rx_pos = 0;
                }
            } else if(bt->rx_pos < (int)sizeof(bt->rx_buf) - 1) {
                bt->rx_buf[bt->rx_pos++] = (char)data[i];
            }
        }
        return BT_SERIAL_BUFFER_SIZE;
    }
    FURI_LOG_D(TAG, "BLE event type: %d", event.event);
    return 0;
}

static void bt_status_changed_cb(BtStatus status, void* context) {
    BtTransport* bt = context;
    if(!bt) return;
    bool new_connected = (status == BtStatusConnected);
    bool changed = (new_connected != bt->connected);
    if(new_connected) {
        FURI_LOG_I(TAG, "BLE client connected, re-setting serial callback");
        bt->connected = true;
        bt->hello_sent = false;
        /* BtSrv opens an RPC connection on connect, which overrides our
         * serial event callback with the RPC data handler.  Re-set our
         * callback here to reclaim control of incoming serial data. */
        ble_profile_serial_set_event_callback(
            bt->profile,
            BT_SERIAL_BUFFER_SIZE,
            bt_serial_event_cb,
            bt);
    } else {
        if(bt->connected) {
            FURI_LOG_I(TAG, "BLE client disconnected");
        }
        bt->connected = false;
        bt->hello_sent = false;
    }
    /* Notify the app of the transition so it can reset its own
     * hello_sent (re-handshake on bridge restart).  Callback runs on
     * the BT stack thread — implementation must not touch UI or call
     * transport_send; dispatch a custom event instead. */
    if(changed && bt->connect_cb) {
        bt->connect_cb(new_connected, bt->connect_ctx);
    }
}

/* ── vtable implementations ────────────────────────────────── */

static void bt_start(Transport* t, TransportRxCallback cb, void* ctx) {
    if(!t) return;
    BtTransport* bt = (BtTransport*)t;
    bt->callback     = cb;
    bt->callback_ctx = ctx;
    bt->rx_pos       = 0;
    bt->connected    = false;
    bt->hello_sent   = false;

    bt->bt = furi_record_open(RECORD_BT);

    /* Disconnect any existing connection before switching profiles */
    bt_disconnect(bt->bt);
    furi_delay_ms(200);

    /* Start the serial profile (restarts BLE core2) */

    bt->profile = bt_profile_start(bt->bt, ble_profile_serial, NULL);
    if(!bt->profile) {
        FURI_LOG_E(TAG, "Failed to start BLE serial profile");
        furi_record_close(RECORD_BT);
        bt->bt = NULL;
        return;
    }

    /* Set RX callback */
    ble_profile_serial_set_event_callback(
        bt->profile,
        BT_SERIAL_BUFFER_SIZE,
        bt_serial_event_cb,
        bt);

    /* Start advertising */
    furi_hal_bt_start_advertising();

    /* Monitor connection status */
    bt_set_status_changed_callback(bt->bt, bt_status_changed_cb, bt);

    FURI_LOG_I(TAG, "BLE serial started, advertising");
}

static void bt_stop(Transport* t) {
    if(!t) return;
    BtTransport* bt = (BtTransport*)t;
    if(!bt->bt) return;

    bt_set_status_changed_callback(bt->bt, NULL, NULL);
    bt_disconnect(bt->bt);
    furi_delay_ms(200);

    /* Restore default profile so built-in BLE RPC works again */
    bt_profile_restore_default(bt->bt);

    furi_record_close(RECORD_BT);
    bt->bt = NULL;
    bt->profile = NULL;
    bt->connected = false;

    FURI_LOG_I(TAG, "BLE serial stopped");
}

static void bt_send(Transport* t, const char* data, int len) {
    if(!t || !data) return;
    BtTransport* bt = (BtTransport*)t;
    if(!bt->profile || !bt->connected) return;

    int offset = 0;
    while(offset < len) {
        int chunk = len - offset;
        if(chunk > BT_TX_CHUNK) chunk = BT_TX_CHUNK;
        ble_profile_serial_tx(bt->profile, (uint8_t*)(data + offset), (uint16_t)chunk);
        offset += chunk;
    }
}

static void bt_free(Transport* t) {
    if(!t) return;
    free(t);
}

/* ── public factory ────────────────────────────────────────── */

Transport* transport_bt_alloc(void) {
    BtTransport* bt  = malloc(sizeof(BtTransport));
    furi_check(bt != NULL);
    memset(bt, 0, sizeof(BtTransport));
    bt->base.start   = bt_start;
    bt->base.stop    = bt_stop;
    bt->base.send    = bt_send;
    bt->base.free_fn = bt_free;
    return (Transport*)bt;
}

/* Optional observer — only the Bridge-mode BT transport fires this.
 * Callers should set it after alloc and before start; we match on the
 * start/stop vtable entries to be safe if called on a non-BT transport
 * (e.g. NUS or USB), in which case we just no-op. */
void transport_bt_set_connect_callback(
    Transport* t, TransportConnectCallback cb, void* context) {
    if(!t || t->start != bt_start) return;
    BtTransport* bt = (BtTransport*)t;
    bt->connect_cb = cb;
    bt->connect_ctx = context;
}
