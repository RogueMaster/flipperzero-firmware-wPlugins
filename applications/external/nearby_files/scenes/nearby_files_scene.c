#include "nearby_files_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const nearby_files_on_enter_handlers[])(void*) = {
#include "nearby_files_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const nearby_files_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "nearby_files_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const nearby_files_on_exit_handlers[])(void* context) = {
#include "nearby_files_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration
const SceneManagerHandlers nearby_files_scene_handlers = {
    .on_enter_handlers = nearby_files_on_enter_handlers,
    .on_event_handlers = nearby_files_on_event_handlers,
    .on_exit_handlers = nearby_files_on_exit_handlers,
    .scene_num = NearbyFilesSceneNum,
};
