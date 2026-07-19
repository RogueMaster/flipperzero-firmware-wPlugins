#include "hermes_scene.h"

/* Generate the scene handler arrays */
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const hermes_scene_on_enter_handlers[])(void*) = {
#include "hermes_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const hermes_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "hermes_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const hermes_scene_on_exit_handlers[])(void* context) = {
#include "hermes_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers hermes_scene_handlers = {
    .on_enter_handlers = hermes_scene_on_enter_handlers,
    .on_event_handlers = hermes_scene_on_event_handlers,
    .on_exit_handlers = hermes_scene_on_exit_handlers,
    .scene_num = HermesSceneNum,
};
