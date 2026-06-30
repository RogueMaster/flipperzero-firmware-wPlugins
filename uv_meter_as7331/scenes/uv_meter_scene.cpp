#include "uv_meter_scene.hpp"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const uv_meter_scene_on_enter_handlers[])(void*) = {
#include "uv_meter_scene_config.hpp"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const uv_meter_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "uv_meter_scene_config.hpp"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const uv_meter_scene_on_exit_handlers[])(void* context) = {
#include "uv_meter_scene_config.hpp"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers uv_meter_scene_handlers = {
    .on_enter_handlers = uv_meter_scene_on_enter_handlers,
    .on_event_handlers = uv_meter_scene_on_event_handlers,
    .on_exit_handlers = uv_meter_scene_on_exit_handlers,
    .scene_num = UVMeterSceneNum,
};