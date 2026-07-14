#include "../pocketlab_i.h"

void pocketlab_scene_badges_on_enter(void* context) {
    PocketLab* app = context;
    badges_view_configure(
        app->badges_view, app->state.completed_mask, app->notifications, app->state.sound != 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewBadges);
}

bool pocketlab_scene_badges_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pocketlab_scene_badges_on_exit(void* context) {
    UNUSED(context);
}
