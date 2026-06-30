#include "../metroflip_i.h"
#include "../api/metroflip/metroflip_api.h"
#include <stdio.h>
enum PopupEvent {
    PopupEventExit,
};

static void metroflip_scene_delete_popup_callback(void* context) {
    Metroflip* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, PopupEventExit);
}

void metroflip_scene_delete_on_enter(void* context) {
    Metroflip* app = context;
    Popup* popup = app->popup;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FURI_LOG_I("PATH", "PATH: %s", app->delete_file_path);
    bool success = storage_simply_remove(storage, app->delete_file_path);
    furi_record_close(RECORD_STORAGE);
    if(success) {
        popup_set_icon(popup, 0, 2, &I_DolphinMafia_119x62);
        popup_set_header(popup, "Deleted", 80, 19, AlignLeft, AlignBottom);
        popup_enable_timeout(popup);
    } else {
        popup_set_icon(popup, 69, 15, &I_WarningDolphinFlip_45x42);
        popup_set_header(popup, "Error!", 13, 22, AlignLeft, AlignBottom);
        popup_disable_timeout(popup);
    }
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, app);
    popup_set_callback(popup, metroflip_scene_delete_popup_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
}

bool metroflip_scene_delete_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case PopupEventExit:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void metroflip_scene_delete_on_exit(void* context) {
    Metroflip* app = context;
    app->delete_file_path[0] = '\0';

    popup_reset(app->popup);
}
