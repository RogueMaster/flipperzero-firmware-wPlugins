#include "src/core/cfs_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const cfs_scene_on_enter_handlers[])(void*) = {
#include "src/scenes/scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const cfs_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "src/scenes/scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const cfs_scene_on_exit_handlers[])(void*) = {
#include "src/scenes/scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers cfs_scene_handlers = {
    .on_enter_handlers = cfs_scene_on_enter_handlers,
    .on_event_handlers = cfs_scene_on_event_handlers,
    .on_exit_handlers = cfs_scene_on_exit_handlers,
    .scene_num = CfsSceneNum,
};
