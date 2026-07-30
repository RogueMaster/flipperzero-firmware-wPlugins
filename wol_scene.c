#include "wol_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const wol_scene_on_enter_handlers[])(void*) = {
#include "wol_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const wol_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "wol_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const wol_scene_on_exit_handlers[])(void*) = {
#include "wol_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers wol_scene_handlers = {
    .on_enter_handlers = wol_scene_on_enter_handlers,
    .on_event_handlers = wol_scene_on_event_handlers,
    .on_exit_handlers = wol_scene_on_exit_handlers,
    .scene_num = WolSceneNum,
};
