#include "zero_buster.h"

/* Main Application Entry Point */
int32_t zero_buster_app(void* p) {
    UNUSED(p);
    ZeroBusterApp* app = malloc(sizeof(ZeroBusterApp));
    memset(app, 0, sizeof(ZeroBusterApp));

    app->layout = Layout_ES; // Default to Spanish ISO keyboard
    app->spoof_profile = Spoof_Logitech_K120; // Default to Logitech K120
    app->active_row = Setting_Payload;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    // Take ownership of CLI VCP to handle USB CDC channel 0
    app->cli_vcp = furi_record_open(RECORD_CLI_VCP);

    // Initial scan of payloads and custom HID from SD card
    scan_payloads(app);
    load_custom_hid_config(app->storage, &app->custom_hid_cfg);

    // Save initial USB config to restore on exit
    app->prev_usb_config = furi_hal_usb_get_config();

    // Setup GUI ViewPort
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, zero_buster_view_draw_callback, app);
    view_port_input_callback_set(app->view_port, zero_buster_view_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Background Worker Thread
    app->worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->worker_thread, "ZeroBusterWorker");
    furi_thread_set_stack_size(app->worker_thread, 3072);
    furi_thread_set_context(app->worker_thread, app);
    furi_thread_set_callback(app->worker_thread, zero_buster_worker_thread);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);

            // Dispatch event to active scene handler
            switch(app->state) {
            case State_Idle:
                running = zero_buster_scene_main_on_event(app, &event);
                break;
            case State_Edit_HID:
                running = zero_buster_scene_edit_hid_on_event(app, &event);
                break;
            case State_About:
                running = zero_buster_scene_about_on_event(app, &event);
                break;
            default:
                running = zero_buster_scene_status_on_event(app, &event);
                break;
            }

            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }

    // Cleanup worker
    app->worker_stop = true;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    // Release USB cleanly and re-enable CLI VCP
    zero_buster_release_usb(app);
    if(app->cli_vcp) {
        furi_record_close(RECORD_CLI_VCP);
    }

    // Cleanup GUI
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    // Cleanup services
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
