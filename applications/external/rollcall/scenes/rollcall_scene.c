#include "../rollcall_i.h"

// Generate on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const rollcall_scene_on_enter_handlers[])(void*) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

// Generate on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const rollcall_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

// Generate on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const rollcall_scene_on_exit_handlers[])(void* context) = {
#include "rollcall_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers rollcall_scene_handlers = {
    .on_enter_handlers = rollcall_scene_on_enter_handlers,
    .on_event_handlers = rollcall_scene_on_event_handlers,
    .on_exit_handlers = rollcall_scene_on_exit_handlers,
    .scene_num = RollCallSceneNum,
};
