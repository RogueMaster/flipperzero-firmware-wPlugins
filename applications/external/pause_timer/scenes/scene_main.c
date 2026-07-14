#include "../pause_timer.h"
#include "../views.h"

void pause_timer_scene_main_on_enter(void* context) {
    PauseTimerApp* app = context;
    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        scene_manager_get_scene_state(app->scene_manager, PauseTimerSceneMain));
}

bool pause_timer_scene_main_on_event(void* context, SceneManagerEvent event) {
    PauseTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void pause_timer_scene_main_on_exit(void* context) {
    UNUSED(context);
}
