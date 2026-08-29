#include "zeromesh_transport.h"
#include "zeromesh_uart.h"
#include "zeromesh_ble.h"
#include "zeromesh_history.h"

#include <bt/bt_service/bt.h>
#include <furi_hal_bt.h>

#define TAG "zeromesh_transport"

static void ble_rx_cb(const uint8_t* data, uint16_t len, void* ctx) {
    ZeroMeshApp* app = ctx;
    if(!app) return;

    app->ble_connected = true;
    app->rx_bytes += len;
    furi_stream_buffer_send(app->rx_stream, data, len, 0);
}

static void ble_status_cb(BtStatus status, void* ctx) {
    ZeroMeshApp* app = ctx;
    if(!app) return;
    app->ble_connected = (status == BtStatusConnected);
}

static bool ble_open(ZeroMeshApp* app) {
    if(app->ble_profile) return true;

    if(!furi_hal_bt_is_active()) {
        log_line(app, "BT is off - enable in Settings");
        set_status(app, "BT off");
        app->ble_failed = true;
        return false;
    }

    app->bt = furi_record_open(RECORD_BT);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    bt_set_status_changed_callback(app->bt, ble_status_cb, app);

    app->ble_profile = bt_profile_start(app->bt, zeromesh_ble_profile, NULL);
    if(!app->ble_profile) {
        FURI_LOG_E(TAG, "bt_profile_start failed");
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        furi_record_close(RECORD_BT);
        app->bt = NULL;
        app->ble_failed = true;
        log_line(app, "BLE profile start failed");
        set_status(app, "BLE failed");
        return false;
    }

    zeromesh_ble_profile_set_rx_callback(app->ble_profile, ble_rx_cb, app);
    furi_hal_bt_start_advertising();

    app->ble_failed = false;
    log_line(app, "BLE advertising, waiting for peer");
    set_status(app, "BLE: advertising");
    return true;
}

static void ble_close(ZeroMeshApp* app) {
    if(!app->ble_profile) return;

    zeromesh_ble_profile_set_rx_callback(app->ble_profile, NULL, NULL);
    app->ble_profile = NULL;
    app->ble_connected = false;

    if(app->bt) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_profile_restore_default(app->bt);
        furi_record_close(RECORD_BT);
        app->bt = NULL;
    }
}

void transport_open(ZeroMeshApp* app) {
    if(!app) return;
    transport_close(app);

    if(app->transport == ZmTransportBle) {
        ble_open(app);
    } else {
        uart_open(app);
    }
}

void transport_close(ZeroMeshApp* app) {
    if(!app) return;
    ble_close(app);
    uart_close(app);
}

void transport_set(ZeroMeshApp* app, ZmTransport t) {
    if(!app || t >= ZmTransportCount) return;
    app->transport = t;
    transport_open(app);
}

bool transport_is_up(ZeroMeshApp* app) {
    if(!app) return false;
    if(app->transport == ZmTransportBle) {

        return app->ble_profile != NULL;
    }
    return app->serial != NULL;
}

void transport_tx(ZeroMeshApp* app, const uint8_t* data, size_t len) {
    if(!app || !data || !len) return;

    if(app->transport == ZmTransportBle) {
        if(!app->ble_profile) return;

        const uint16_t chunk_max = zeromesh_ble_profile_max_frame();
        size_t sent = 0;
        while(sent < len) {
            uint16_t n = (uint16_t)((len - sent) > chunk_max ? chunk_max : (len - sent));
            if(!zeromesh_ble_profile_tx(app->ble_profile, data + sent, n)) {

                FURI_LOG_W(TAG, "notify dropped (%u bytes)", n);
                return;
            }
            sent += n;
        }
        return;
    }

    if(app->serial) {
        furi_hal_serial_tx(app->serial, data, len);
    }
}

const char* transport_name(ZeroMeshApp* app) {
    if(!app) return "?";
    if(app->transport == ZmTransportBle) {
        if(app->ble_failed) return "BLE failed";
        if(!app->ble_profile) return "BLE off";
        return app->ble_connected ? "BLE linked" : "BLE advertising";
    }
    return (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART";
}
