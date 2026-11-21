#include "../../rf_studio_i.h"

#define MESSAGE_TIMEOUT_MS 4000

static void rfs_message_timeout(void* context) {
    RfStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventPopupDone);
}

void rfs_scene_message_on_enter(void* context) {
    RfStudioApp* app = context;
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(
        popup, furi_string_get_cstr(app->message_title), 64, 2, AlignCenter, AlignTop);
    popup_set_text(popup, furi_string_get_cstr(app->message_text), 64, 16, AlignCenter, AlignTop);
    popup_set_context(popup, app);
    popup_set_callback(popup, rfs_message_timeout);
    popup_set_timeout(popup, MESSAGE_TIMEOUT_MS);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewPopup);
}

static void rfs_message_return(RfStudioApp* app) {
    if(app->message_return == RfsSceneNum || !scene_manager_search_and_switch_to_previous_scene(
                                                 app->scene_manager, app->message_return)) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool rfs_scene_message_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == RfsEventPopupDone)) {
        rfs_message_return(app);
        return true;
    }
    return false;
}

void rfs_scene_message_on_exit(void* context) {
    RfStudioApp* app = context;
    popup_reset(app->popup);
}
