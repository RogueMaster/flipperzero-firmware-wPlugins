#include "meshcore_scene.h"

/* Handler tables, generated from meshcore_scene_config.h. Order matches the
 * MeshCoreSceneId enum, which is what SceneManager indexes by. */

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const meshcore_scene_on_enter_handlers[])(void*) = {
#include "meshcore_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const meshcore_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "meshcore_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const meshcore_scene_on_exit_handlers[])(void*) = {
#include "meshcore_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers meshcore_scene_handlers = {
    .on_enter_handlers = meshcore_scene_on_enter_handlers,
    .on_event_handlers = meshcore_scene_on_event_handlers,
    .on_exit_handlers = meshcore_scene_on_exit_handlers,
    .scene_num = MeshCoreSceneNum,
};
