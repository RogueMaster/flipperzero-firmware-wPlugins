#include "pc_monitor.h"

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    PcMonitorApp* app = ctx;

    switch(app->usb_state) {
    case UsbStateWaiting:
        draw_connect_view(canvas);
        // Use a different string or icon for USB if possible, but existing view is fine for now
        // Maybe draw "Connect USB" text manually if connect_view is hardcoded to "Connect BT"
        break;

    case UsbStateActive:
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

static void cdc_rx_callback(void* context) {
    furi_assert(context);
    PcMonitorApp* app = context;
    app->new_data_received = true;
}

static PcMonitorApp* pc_monitor_alloc() {
    PcMonitorApp* app = malloc(sizeof(PcMonitorApp));
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    
    // USB callback setup
    // We use the default CDC channel 0
    static CdcCallbacks usb_cdc_callbacks = {
        .tx_ep_callback = NULL,
        .rx_ep_callback = cdc_rx_callback,
        .state_callback = NULL,
        .ctrl_line_callback = NULL,
        .config_callback = NULL,
        .break_callback = NULL,
    };
    furi_hal_cdc_set_callbacks(0, &usb_cdc_callbacks, app);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);
    
    app->usb_state = UsbStateWaiting;
    
    return app;
}

static void pc_monitor_free(PcMonitorApp* app) {
    // Restore default CDC callbacks (usually CLI)
    // There isn't a "restore default" function easily accessible, but passing NULL might disable ours.
    // However, the best practice is to reset to NULL or let the system handle it if we didn't save the old one.
    // But since we can't easily get the old one, we just clear ours.
    furi_hal_cdc_set_callbacks(0, NULL, NULL);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pc_monitor_app(void* p) {
    UNUSED(p);
    
    // Disable expansion modules to avoid interference? Not strictly necessary for USB.
    
    PcMonitorApp* app = pc_monitor_alloc();

    FURI_LOG_D(TAG, "USB Monitor started");

    // Main loop
    InputEvent event;
    while(true) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) { // 100ms timeout for checking connection state
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }

        if(app->new_data_received) {
            app->new_data_received = false;
            
            uint8_t buffer[sizeof(DataStruct)];
            // Read data from CDC interface 0
            int32_t len = furi_hal_cdc_receive(0, buffer, sizeof(DataStruct));
            
            if(len == sizeof(DataStruct)) {
                memcpy(&app->data, buffer, sizeof(DataStruct));
                app->usb_state = UsbStateActive;
                app->last_packet = furi_hal_rtc_get_timestamp();
                
                notification_message(app->notification, &sequence_display_backlight_on);
                notification_message(app->notification, &sequence_blink_blue_10);
                view_port_update(app->view_port);
            }
        }

        if(app->usb_state == UsbStateActive &&
           (furi_hal_rtc_get_timestamp() - app->last_packet > 5)) {
            app->usb_state = UsbStateLost;
        }
        
        // Ensure connected check

    }

    pc_monitor_free(app);

    return 0;
}
