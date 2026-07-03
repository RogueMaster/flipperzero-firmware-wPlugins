#include "../passy_i.h"

void passy_scene_delete_success_popup_callback(void* context) {
    Passy* passy = context;
    view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventViewExit);
}

void passy_scene_delete_success_on_enter(void* context) {
    Passy* passy = context;

    // Setup view
    Popup* popup = passy->popup;
    popup_set_icon(popup, 0, 2, &I_DolphinMafia_119x62);
    popup_set_header(popup, "Deleted", 80, 19, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, passy);
    popup_set_callback(popup, passy_scene_delete_success_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewPopup);
}

bool passy_scene_delete_success_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PassyCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                passy->scene_manager, PassySceneMainMenu);
        }
    }
    return consumed;
}

void passy_scene_delete_success_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    popup_reset(passy->popup);
}
