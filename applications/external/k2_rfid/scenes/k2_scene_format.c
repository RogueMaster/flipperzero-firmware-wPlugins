#include "../k2_rfid_app.h"
#include "k2_scene.h"

static void k2_scene_format_worker_callback(K2WorkerEvent event, void* context) {
    K2RfidApp* app = context;
    uint32_t custom_event = 0;
    switch (event) {
    case K2WorkerEventCardDetected:
        custom_event = K2CustomEventCardDetected;
        break;
    case K2WorkerEventSuccess:
        custom_event = K2CustomEventSuccess;
        break;
    case K2WorkerEventFormatFailed:
    default:
        custom_event = K2CustomEventFailed;
        break;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
}

void k2_scene_format_on_enter(void* context) {
    K2RfidApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    popup_set_header(popup, "Format Tag", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Erase CFS Tag?\nResets to blank keys.\n\nHold tag to back...", 64, 25, AlignCenter, AlignTop);

    k2_worker_set_callback(app->worker, k2_scene_format_worker_callback, app);
    k2_worker_start_format(app->worker);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
}

bool k2_scene_format_on_event(void* context, SceneManagerEvent event) {
    K2RfidApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if (event.event == K2CustomEventCardDetected) {
            popup_set_text(app->popup, "Tag detected!\nWiping sectors...", 64, 30, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_blink_yellow_10);
        } else if (event.event == K2CustomEventSuccess) {
            popup_set_header(app->popup, "Success!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Tag formatted!\nReset to factory keys.\nPress Back", 64, 25, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_success);
        } else if (event.event == K2CustomEventFailed) {
            popup_set_header(app->popup, "Format Failed!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Could not format tag.\nEnsure tag is Mifare 1K\nand in range.", 64, 25, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_error);
        }
    }

    return consumed;
}

void k2_scene_format_on_exit(void* context) {
    K2RfidApp* app = context;
    k2_worker_stop(app->worker);
    k2_worker_set_callback(app->worker, NULL, NULL);
    popup_reset(app->popup);
}
