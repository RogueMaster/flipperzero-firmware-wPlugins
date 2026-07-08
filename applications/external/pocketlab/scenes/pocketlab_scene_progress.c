#include "../pocketlab_i.h"

void pocketlab_scene_progress_on_enter(void* context) {
    PocketLab* app = context;
    progress_view_configure(
        app->progress_view,
        app->state.level,
        app->state.xp,
        pocketlab_completed_count(app),
        pocketlab_labs_count);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewProgress);
}

bool pocketlab_scene_progress_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pocketlab_scene_progress_on_exit(void* context) {
    UNUSED(context);
}
