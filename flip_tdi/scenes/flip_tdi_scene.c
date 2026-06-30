#include "../flip_tdi_app_i.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const flip_tdi_scene_on_enter_handlers[])(void*) = {
#include "flip_tdi_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const flip_tdi_scene_on_event_handlers[])(void* context, SceneManagerEvent event) =
    {
#include "flip_tdi_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const flip_tdi_scene_on_exit_handlers[])(void* context) = {
#include "flip_tdi_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers flip_tdi_scene_handlers = {
    .on_enter_handlers = flip_tdi_scene_on_enter_handlers,
    .on_event_handlers = flip_tdi_scene_on_event_handlers,
    .on_exit_handlers = flip_tdi_scene_on_exit_handlers,
    .scene_num = FlipTDISceneNum,
};
