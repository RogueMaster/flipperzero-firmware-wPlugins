#include "../weebo_i.h"

void weebo_scene_file_select_on_enter(void* context) {
    Weebo* weebo = context;

    // Process file_select return
    weebo_set_loading_callback(weebo, weebo_show_loading_popup, weebo);
    if(weebo_file_select(weebo)) {
        scene_manager_next_scene(weebo->scene_manager, WeeboSceneSavedMenu);
    } else {
        scene_manager_search_and_switch_to_previous_scene(
            weebo->scene_manager, WeeboSceneMainMenu);
    }
    weebo_set_loading_callback(weebo, NULL, weebo);
}

bool weebo_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void weebo_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
