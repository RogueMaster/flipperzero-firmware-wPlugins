#include "unitemp.h"

extern const Icon I_DolphinMafia_119x62;

static void unitemp_scene_delete_success_popup_callback(void* context) {
    UnitempApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventBack);
}

void unitemp_scene_delete_success_on_enter(void* context) {
    UnitempApp* app = context;
    Popup* popup = app->popup;

    popup_set_icon(popup, 0, 2, &I_DolphinMafia_119x62);
    popup_set_header(popup, "Deleted", 80, 19, AlignLeft, AlignBottom);
    popup_set_callback(popup, unitemp_scene_delete_success_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 1500);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewPopup);
}

bool unitemp_scene_delete_success_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == CustomEventBack) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, UnitempSceneMonitor);
        }
    }

    return consumed;
}

void unitemp_scene_delete_success_on_exit(void* context) {
    UnitempApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
}
