#include "../pocketlab_i.h"

void pocketlab_scene_labs_on_enter(void* context) {
    PocketLab* app = context;
    const uint32_t selected =
        scene_manager_get_scene_state(app->scene_manager, PocketLabSceneLabs);
    labs_list_view_configure(
        app->labs_list_view,
        app->state.completed_mask,
        selected,
        app->notifications,
        app->state.sound != 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewLabs);
}

bool pocketlab_scene_labs_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event < pocketlab_labs_count) {
        app->current_lab = &pocketlab_labs[event.event];
        scene_manager_next_scene(app->scene_manager, PocketLabSceneLesson);
        return true;
    }

    return false;
}

void pocketlab_scene_labs_on_exit(void* context) {
    PocketLab* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, PocketLabSceneLabs, labs_list_view_get_selected(app->labs_list_view));
}
