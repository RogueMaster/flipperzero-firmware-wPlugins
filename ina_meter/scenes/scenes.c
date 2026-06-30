#include "scenes.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(name, id) scene_##name##_on_enter,
void (*const on_enter_handlers[])(void*) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(name, id) scene_##name##_on_event,
bool (*const on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(name, id) scene_##name##_on_exit,
void (*const on_exit_handlers[])(void* context) = {
#include "scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers = on_exit_handlers,
    .scene_num = SceneNum,
};