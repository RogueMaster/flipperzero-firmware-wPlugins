#include "ear_scene.h"

#define ADD_SCENE(name) ear_scene_##name##_on_enter,
static void (*const ear_on_enter_handlers[])(void*) = {
#include "ear_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(name) ear_scene_##name##_on_event,
static bool (*const ear_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "ear_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(name) ear_scene_##name##_on_exit,
static void (*const ear_on_exit_handlers[])(void*) = {
#include "ear_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers ear_scene_handlers = {
    .on_enter_handlers = ear_on_enter_handlers,
    .on_event_handlers = ear_on_event_handlers,
    .on_exit_handlers = ear_on_exit_handlers,
    .scene_num = EarSceneNum,
};
