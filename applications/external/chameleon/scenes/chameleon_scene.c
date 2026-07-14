#include "chameleon_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const chameleon_on_enter_handlers[])(void*) = {
#include "chameleon_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const chameleon_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "chameleon_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const chameleon_on_exit_handlers[])(void* context) = {
#include "chameleon_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration
const SceneManagerHandlers chameleon_scene_handlers = {
    .on_enter_handlers = chameleon_on_enter_handlers,
    .on_event_handlers = chameleon_on_event_handlers,
    .on_exit_handlers = chameleon_on_exit_handlers,
    .scene_num = ChameleonSceneNum,
};
