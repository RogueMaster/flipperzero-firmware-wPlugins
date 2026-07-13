#include "../jayx.h"
#include <furi_hal_bt.h>

static uint16_t jayx_bt_serial_event(SerialServiceEvent event, void* context) {
    JayxApp* app = context;
    furi_assert(app);

    if(event.event == SerialServiceEventTypeDataReceived) {
        if(event.data.buffer && event.data.size > 0) {
            jayx_feed_bytes(app, event.data.buffer, event.data.size);
            app->rx_pending = true;
        }
        if(app->ble_profile) {
            ble_profile_serial_notify_buffer_is_empty(app->ble_profile);
        }
        return (uint16_t)furi_stream_buffer_spaces_available(app->rx_stream);
    }

    if(event.event == SerialServiceEventTypesBleResetRequest) {
        FURI_LOG_W(TAG, "BLE reset requested");
    }

    return 0;
}

static void jayx_bt_status_changed(BtStatus status, void* context) {
    JayxApp* app = context;
    furi_assert(app);
    app->bt_connected = (status == BtStatusConnected);

    /* After a drop, start advertising again so the PC can rescan */
    if(status != BtStatusConnected && app->ble_profile) {
        furi_hal_bt_start_advertising();
    }

    if(app->view_port) {
        view_port_update(app->view_port);
    }
}

void jayx_bt_start(JayxApp* app) {
    app->bt = furi_record_open(RECORD_BT);
    app->bt_connected = false;

    bt_disconnect(app->bt);
    /* Give the stack time to fully drop the previous central */
    furi_delay_ms(500);

    app->ble_profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(!app->ble_profile) {
        FURI_LOG_E(TAG, "Failed to start BLE serial profile");
        return;
    }

    /* App buffer for flow-control (must fit incoming serial chunks) */
    ble_profile_serial_set_event_callback(
        app->ble_profile, JAYX_RX_STREAM_SIZE, jayx_bt_serial_event, app);
    ble_profile_serial_set_rpc_active(app->ble_profile, false);

    bt_set_status_changed_callback(app->bt, jayx_bt_status_changed, app);
    furi_hal_bt_start_advertising();
    FURI_LOG_I(TAG, "BLE serial advertising");
}

void jayx_bt_stop(JayxApp* app) {
    if(app->bt) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
    }

    if(app->ble_profile) {
        ble_profile_serial_set_event_callback(app->ble_profile, 0, NULL, NULL);
        app->ble_profile = NULL;
    }

    if(app->bt) {
        bt_disconnect(app->bt);
        bt_profile_restore_default(app->bt);
        furi_record_close(RECORD_BT);
        app->bt = NULL;
    }

    app->bt_connected = false;
}
