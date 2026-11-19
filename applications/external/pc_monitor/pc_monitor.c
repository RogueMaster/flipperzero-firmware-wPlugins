#include "pc_monitor.h"

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    PcMonitorApp* app = ctx;

    switch(app->bt_state) {
    case BtStateWaiting:
        draw_connect_view(canvas);
        break;

    case BtStateRecieving:
        draw_bars_view(canvas, app);
        break;

    default:
        draw_status_view(canvas, app);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static uint16_t bt_serial_callback(SerialServiceEvent event, void* ctx) {
    furi_assert(ctx);
    PcMonitorApp* app = ctx;

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_D(
            TAG,
            "SerialServiceEventTypeDataReceived. Size: %u/%u. Data: %s",
            event.data.size,
            sizeof(DataStruct),
            (char*)event.data.buffer);

        if(event.data.size == sizeof(DataStruct)) {
            memcpy(&app->data, event.data.buffer, sizeof(DataStruct));
            app->bt_state = BtStateRecieving;
            app->last_packet = furi_hal_rtc_get_timestamp();

            // Elegant solution, the backlight is only on when there is continuous communication
            notification_message(app->notification, &sequence_display_backlight_on);

            notification_message(app->notification, &sequence_blink_blue_10);

            // Nothing else asks for a repaint, so the new numbers only reach the
            // screen if we request it here
            view_port_update(app->view_port);
        }
    }

    return 0;
}

static PcMonitorApp* pc_monitor_alloc() {
    PcMonitorApp* app = malloc(sizeof(PcMonitorApp));
    // The draw callback switches on this and runs as soon as the view port is added,
    // so it cannot be left as whatever malloc() handed us
    app->bt_state = BtStateWaiting;
    // draw_bars_view() divides by lines_count and only assigns it on the way out, so the
    // first bars frame reads it before anything has written it. Start at the number of
    // rows it can draw; that frame then corrects it to however many the PC actually sent.
    app->lines_count = 4;
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);
    return app;
}

static void pc_monitor_free(PcMonitorApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    free(app);
}

int32_t pc_monitor_app(void* p) {
    UNUSED(p);
    PcMonitorApp* app = pc_monitor_alloc();

    bt_disconnect(app->bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(".bt_serial.keys"));

    BleProfileSerialParams params = {
        .device_name_prefix = "PC Mon",
        .mac_xor = 0x0002,
    };
    app->ble_serial_profile = bt_profile_start(app->bt, ble_profile_serial, &params);

    furi_check(app->ble_serial_profile);

    ble_profile_serial_set_event_callback(
        app->ble_serial_profile, BT_SERIAL_BUFFER_SIZE, bt_serial_callback, app);
    furi_hal_bt_start_advertising();

    app->bt_state = BtStateWaiting;
    view_port_update(app->view_port);
    FURI_LOG_D(TAG, "Bluetooth is active!");

    // Main loop
    InputEvent event;
    while(true) {
        if(furi_message_queue_get(app->event_queue, &event, 1) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }

        if(app->bt_state == BtStateRecieving &&
           (furi_hal_rtc_get_timestamp() - app->last_packet > 5)) {
            app->bt_state = BtStateLost;
            view_port_update(app->view_port);
        }
    }

    ble_profile_serial_set_event_callback(app->ble_serial_profile, 0, NULL, NULL);

    bt_disconnect(app->bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    bt_keys_storage_set_default_path(app->bt);

    furi_check(bt_profile_restore_default(app->bt));

    pc_monitor_free(app);

    return 0;
}
