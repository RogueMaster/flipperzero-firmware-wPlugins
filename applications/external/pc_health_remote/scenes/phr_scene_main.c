#include "../phr_app.h"
#include "phr_scene.h"

void phr_scene_main_on_enter(void* context) {
    PhrApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, PhrViewDashboard);
}

bool phr_scene_main_on_event(void* context, SceneManagerEvent event) {
    PhrApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PhrEventOpenSettings) {
            scene_manager_next_scene(app->scene_manager, PhrSceneSettings);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->pending_count > 0) {
            scene_manager_next_scene(app->scene_manager, PhrSceneAlert);
            return true;
        }
    }
    return false; // Back exits the app
}

void phr_scene_main_on_exit(void* context) {
    UNUSED(context);
}
