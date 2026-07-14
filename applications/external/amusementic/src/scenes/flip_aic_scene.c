#include "flip_aic_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(name, id) flip_aic_scene_##name##_on_enter,
void (*const flip_aic_on_enter_handlers[])(void*) = {
#include "flip_aic_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(name, id) flip_aic_scene_##name##_on_event,
bool (*const flip_aic_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "flip_aic_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(name, id) flip_aic_scene_##name##_on_exit,
void (*const flip_aic_on_exit_handlers[])(void* context) = {
#include "flip_aic_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers flip_aic_scene_handlers = {
    .on_enter_handlers = flip_aic_on_enter_handlers,
    .on_event_handlers = flip_aic_on_event_handlers,
    .on_exit_handlers = flip_aic_on_exit_handlers,
    .scene_num = FlipAICSceneNum,
};
