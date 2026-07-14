#include "../chameleon_app_i.h"

void chameleon_scene_start_on_enter(void* context) {
    ChameleonApp* app = context;

    // Show loading screen briefly
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewLoading);

    // Auto-navigate to main menu after a short delay
    scene_manager_next_scene(app->scene_manager, ChameleonSceneMainMenu);
}

bool chameleon_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_start_on_exit(void* context) {
    UNUSED(context);
}
