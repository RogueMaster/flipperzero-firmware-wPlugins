#include "../k2_rfid_app.h"
#include "k2_scene.h"

static void k2_scene_write_worker_callback(K2WorkerEvent event, void* context) {
    K2RfidApp* app = context;
    uint32_t custom_event = 0;
    switch(event) {
    case K2WorkerEventCardDetected:
        custom_event = K2CustomEventCardDetected;
        break;
    case K2WorkerEventSuccess:
        custom_event = K2CustomEventSuccess;
        break;
    case K2WorkerEventWriteFailed:
    default:
        custom_event = K2CustomEventFailed;
        break;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
}

void k2_scene_write_on_enter(void* context) {
    K2RfidApp* app = context;
    Popup* popup = app->popup;

    /* Ensure valid spool configuration */
    if(app->config.material_id[0] == '\0') {
        k2_rfid_app_set_default_config(app);
    } else {
        k2_rfid_app_sync_config(app);
    }

    const K2Material* mat = k2_db_find_material_by_id(app->config.material_id);
    const char* mat_name = (mat && mat->name) ? mat->name : "Hyper PLA";
    const char* col_name = k2_db_find_closest_color_name(app->config.color_hex);
    if(!col_name || strlen(col_name) == 0) {
        col_name = "White";
    }
    const char* weight = k2_db_find_weight_label_by_code(app->config.weight_code);
    if(!weight || strlen(weight) == 0) {
        weight = "1 KG";
    }

    FuriString* str = furi_string_alloc();
    furi_string_printf(str, "Write: %s\n%s - %s\nHold tag to back...", mat_name, col_name, weight);

    popup_reset(popup);
    popup_set_header(popup, "Write Spool Tag", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, furi_string_get_cstr(str), 64, 25, AlignCenter, AlignTop);
    furi_string_free(str);

    k2_worker_set_callback(app->worker, k2_scene_write_worker_callback, app);
    k2_worker_start_write(app->worker, &app->config);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
}

bool k2_scene_write_on_event(void* context, SceneManagerEvent event) {
    K2RfidApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == K2CustomEventCardDetected) {
            popup_set_text(
                app->popup, "Tag detected!\nWriting blocks...", 64, 30, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_blink_yellow_10);
        } else if(event.event == K2CustomEventSuccess) {
            popup_set_header(app->popup, "Success!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Spool tag written!\nKeys encrypted for CFS.\nPress Back",
                64,
                25,
                AlignCenter,
                AlignTop);
            notification_message(app->notifications, &sequence_success);
        } else if(event.event == K2CustomEventFailed) {
            popup_set_header(app->popup, "Write Failed!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Could not write tag.\nEnsure it is a writable\nMifare Classic 1K.",
                64,
                25,
                AlignCenter,
                AlignTop);
            notification_message(app->notifications, &sequence_error);
        }
    }

    return consumed;
}

void k2_scene_write_on_exit(void* context) {
    K2RfidApp* app = context;
    k2_worker_stop(app->worker);
    k2_worker_set_callback(app->worker, NULL, NULL);
    popup_reset(app->popup);
}
