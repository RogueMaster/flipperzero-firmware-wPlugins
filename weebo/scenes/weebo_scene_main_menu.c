#include "../weebo_i.h"

#define TAG "SceneMainMenu"

enum SubmenuIndex {
    SubmenuIndexSaved = 0,
    SubmenuIndexAcknowledgements,
};

void weebo_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    Weebo* weebo = context;
    view_dispatcher_send_custom_event(weebo->view_dispatcher, index);
}

void weebo_scene_main_menu_on_enter(void* context) {
    Weebo* weebo = context;
    Submenu* submenu = weebo->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu, "Saved", SubmenuIndexSaved, weebo_scene_main_menu_submenu_callback, weebo);
    submenu_add_item(
        submenu,
        "Acknowledgements",
        SubmenuIndexAcknowledgements,
        weebo_scene_main_menu_submenu_callback,
        weebo);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(weebo->scene_manager, WeeboSceneMainMenu));
    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewMenu);
}

bool weebo_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(weebo->scene_manager, WeeboSceneMainMenu, event.event);
        if(event.event == SubmenuIndexSaved) {
            scene_manager_next_scene(weebo->scene_manager, WeeboSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexAcknowledgements) {
            scene_manager_next_scene(weebo->scene_manager, WeeboSceneAcknowledgements);
            consumed = true;
        }
    }

    return consumed;
}

void weebo_scene_main_menu_on_exit(void* context) {
    Weebo* weebo = context;

    submenu_reset(weebo->submenu);
}
