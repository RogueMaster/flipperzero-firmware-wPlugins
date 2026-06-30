#include "../weebo_i.h"

void weebo_scene_keys_missing_popup_callback(void* context) {
    Weebo* weebo = context;
    view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventViewExit);
}

void weebo_scene_keys_missing_on_enter(void* context) {
    Weebo* weebo = context;

    Popup* popup = weebo->popup;

    popup_set_header(popup, "key_retail.bin missing", 58, 28, AlignCenter, AlignCenter);
    // popup_set_text(popup, "words", 64, 36, AlignCenter, AlignTop);
    popup_set_context(weebo->popup, weebo);
    popup_set_callback(popup, weebo_scene_keys_missing_popup_callback);

    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewPopup);
}

bool weebo_scene_keys_missing_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(weebo->scene_manager, WeeboSceneKeysMissing, event.event);
        if(event.event == WeeboCustomEventViewExit) {
            while(scene_manager_previous_scene(weebo->scene_manager))
                ;
            consumed = true;
        }
    }

    return consumed;
}

void weebo_scene_keys_missing_on_exit(void* context) {
    Weebo* weebo = context;
    popup_reset(weebo->popup);
}
