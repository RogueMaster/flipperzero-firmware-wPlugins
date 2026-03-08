#include "scene_functions.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##id##_on_enter,
void (*const cancommander_scene_on_enter_handlers[])(void*) = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##id##_on_event,
bool (*const cancommander_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##id##_on_exit,
void (*const cancommander_scene_on_exit_handlers[])(void*) = {
#include "scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers cancommander_scene_handlers = {
    .on_enter_handlers = cancommander_scene_on_enter_handlers,
    .on_event_handlers = cancommander_scene_on_event_handlers,
    .on_exit_handlers = cancommander_scene_on_exit_handlers,
    .scene_num = cancommander_scene_num,
};
