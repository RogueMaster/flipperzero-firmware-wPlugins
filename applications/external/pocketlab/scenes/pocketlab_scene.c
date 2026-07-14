#include "pocketlab_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const pocketlab_scene_on_enter_handlers[])(void*) = {
#include "pocketlab_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const pocketlab_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "pocketlab_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const pocketlab_scene_on_exit_handlers[])(void* context) = {
#include "pocketlab_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers pocketlab_scene_handlers = {
    .on_enter_handlers = pocketlab_scene_on_enter_handlers,
    .on_event_handlers = pocketlab_scene_on_event_handlers,
    .on_exit_handlers = pocketlab_scene_on_exit_handlers,
    .scene_num = PocketLabSceneNum,
};
