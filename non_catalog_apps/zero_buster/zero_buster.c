#include "zero_buster.h"

int32_t zero_buster_app(void* p) {
    UNUSED(p);
    ZeroBusterApp* app = malloc(sizeof(ZeroBusterApp));
    memset(app, 0, sizeof(ZeroBusterApp));

    app->spoof_profile = Spoof_Logitech_K120;
    app->active_row = Setting_Payload;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->cli_vcp = furi_record_open(RECORD_CLI_VCP);

    scan_payloads(app);
    scan_layouts(app);
    load_custom_hid_config(app->storage, &app->custom_hid_cfg);

    app->prev_usb_config = furi_hal_usb_get_config();

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, zero_buster_view_draw_callback, app);
    view_port_input_callback_set(app->view_port, zero_buster_view_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

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

    app->worker_stop = true;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    zero_buster_release_usb(app);
    if(app->cli_vcp) {
        furi_record_close(RECORD_CLI_VCP);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
