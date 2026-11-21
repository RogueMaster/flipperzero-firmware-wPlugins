#include "rfs_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const rfs_on_enter_handlers[])(void*) = {
#include "rfs_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const rfs_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "rfs_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const rfs_on_exit_handlers[])(void* context) = {
#include "rfs_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers rfs_scene_handlers = {
    .on_enter_handlers = rfs_on_enter_handlers,
    .on_event_handlers = rfs_on_event_handlers,
    .on_exit_handlers = rfs_on_exit_handlers,
    .scene_num = RfsSceneNum,
};
