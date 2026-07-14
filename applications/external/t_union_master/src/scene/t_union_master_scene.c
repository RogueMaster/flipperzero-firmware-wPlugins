#include "t_union_master_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const tum_on_enter_handlers[])(void*) = {
#include "t_union_master_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const tum_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "t_union_master_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const tum_on_exit_handlers[])(void* context) = {
#include "t_union_master_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers tum_scene_handlers = {
    .on_enter_handlers = tum_on_enter_handlers,
    .on_event_handlers = tum_on_event_handlers,
    .on_exit_handlers = tum_on_exit_handlers,
    .scene_num = TUM_SceneNum,
};
