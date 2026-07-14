#include "../seos_i.h"

void seos_scene_delete_success_popup_callback(void* context) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventViewExit);
}

void seos_scene_delete_success_on_enter(void* context) {
    Seos* seos = context;

    // Setup view
    Popup* popup = seos->popup;
    popup_set_icon(popup, 0, 2, &I_DolphinMafia_115x62);
    popup_set_header(popup, "Deleted", 83, 19, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, seos);
    popup_set_callback(popup, seos_scene_delete_success_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_delete_success_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneMainMenu);
        }
    }
    return consumed;
}

void seos_scene_delete_success_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    popup_reset(seos->popup);
}
