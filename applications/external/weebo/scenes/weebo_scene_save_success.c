#include "../weebo_i.h"
#include <dolphin/dolphin.h>

void weebo_scene_save_success_popup_callback(void* context) {
    Weebo* weebo = context;
    view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventViewExit);
}

void weebo_scene_save_success_on_enter(void* context) {
    Weebo* weebo = context;
    dolphin_deed(DolphinDeedNfcSave);

    // Setup view
    Popup* popup = weebo->popup;
    popup_set_icon(popup, 32, 5, &I_DolphinNice_96x59);
    popup_set_header(popup, "Saved!", 13, 22, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, weebo);
    popup_set_callback(popup, weebo_scene_save_success_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewPopup);
}

bool weebo_scene_save_success_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WeeboCustomEventViewExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                weebo->scene_manager, WeeboSceneSavedMenu);
        }
    }
    return consumed;
}

void weebo_scene_save_success_on_exit(void* context) {
    Weebo* weebo = context;

    // Clear view
    popup_reset(weebo->popup);
}
