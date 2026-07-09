#include "../pocketlab_i.h"

void pocketlab_scene_about_on_enter(void* context) {
    PocketLab* app = context;
    about_view_configure(app->about_view, app->notifications, app->state.sound != 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewAbout);
}

bool pocketlab_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pocketlab_scene_about_on_exit(void* context) {
    UNUSED(context);
}
