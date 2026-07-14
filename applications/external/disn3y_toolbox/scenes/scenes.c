#include "scenes.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const disn3y_toolbox_app_on_enter_handlers[])(void*) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const disn3y_toolbox_app_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const disn3y_toolbox_app_on_exit_handlers[])(void* context) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers disn3y_toolbox_app_scene_handlers = {
    .on_enter_handlers = disn3y_toolbox_app_on_enter_handlers,
    .on_event_handlers = disn3y_toolbox_app_on_event_handlers,
    .on_exit_handlers = disn3y_toolbox_app_on_exit_handlers,
    .scene_num = Disn3yToolboxAppSceneNum,
};
