#include "../k2_rfid_app.h"
#include "k2_scene.h"

static void k2_scene_scan_worker_callback(K2WorkerEvent event, void* context) {
    K2RfidApp* app = context;
    uint32_t custom_event = 0;
    switch (event) {
    case K2WorkerEventCardDetected:
        custom_event = K2CustomEventCardDetected;
        break;
    case K2WorkerEventSuccess:
        custom_event = K2CustomEventSuccess;
        break;
    case K2WorkerEventAuthFailed:
        custom_event = K2CustomEventAuthFailed;
        break;
    case K2WorkerEventReadFailed:
    default:
        custom_event = K2CustomEventFailed;
        break;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
}

void k2_scene_scan_on_enter(void* context) {
    K2RfidApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    popup_set_header(popup, "Scan Spool Tag", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Hold tag against\nFlipper's back...", 64, 30, AlignCenter, AlignTop);

    k2_worker_set_callback(app->worker, k2_scene_scan_worker_callback, app);
    k2_worker_start_scan(app->worker);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
}

bool k2_scene_scan_on_event(void* context, SceneManagerEvent event) {
    K2RfidApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if (event.event == K2CustomEventCardDetected) {
            popup_set_text(app->popup, "Tag detected!\nDecrypting...", 64, 30, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_blink_yellow_10);
        } else if (event.event == K2CustomEventSuccess) {
            const K2SpoolInfo* info = k2_worker_get_last_info(app->worker);
            if (info) {
                app->last_spool = *info;
            }
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, K2SceneTagInfo);
        } else if (event.event == K2CustomEventAuthFailed || event.event == K2CustomEventFailed) {
            popup_set_text(app->popup, "Read Failed!\nNot a valid CFS tag\nor auth failed.", 64, 25, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_error);
        }
    }

    return consumed;
}

void k2_scene_scan_on_exit(void* context) {
    K2RfidApp* app = context;
    k2_worker_stop(app->worker);
    k2_worker_set_callback(app->worker, NULL, NULL);
    popup_reset(app->popup);
}
