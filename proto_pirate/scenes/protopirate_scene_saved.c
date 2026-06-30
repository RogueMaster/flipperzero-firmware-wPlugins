// scenes/protopirate_scene_saved.c
#include "../protopirate_app_i.h"

#define TAG "ProtoPirateSceneSaved"

void protopirate_scene_saved_on_enter(void* context) {
    ProtoPirateApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

bool protopirate_scene_saved_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void protopirate_scene_saved_on_exit(void* context) {
    UNUSED(context);
}
