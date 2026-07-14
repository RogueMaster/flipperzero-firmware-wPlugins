#include "vk_thermo_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const vk_thermo_on_enter_handlers[])(void*) = {
#include "vk_thermo_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const vk_thermo_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "vk_thermo_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const vk_thermo_on_exit_handlers[])(void* context) = {
#include "vk_thermo_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers vk_thermo_scene_handlers = {
    .on_enter_handlers = vk_thermo_on_enter_handlers,
    .on_event_handlers = vk_thermo_on_event_handlers,
    .on_exit_handlers = vk_thermo_on_exit_handlers,
    .scene_num = VkThermoSceneNum,
};
