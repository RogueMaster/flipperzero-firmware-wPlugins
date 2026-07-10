#include "include/scenes/gurpil_scene.h"

// Generate scene on_enter handler array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const gurpil_on_enter_handlers[])(void *) = {
#include "include/scenes/gurpil_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handler array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const gurpil_on_event_handlers[])(void *context, SceneManagerEvent event) = {
#include "include/scenes/gurpil_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handler array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const gurpil_on_exit_handlers[])(void *context) = {
#include "include/scenes/gurpil_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers gurpil_scene_handlers = {
    .on_enter_handlers = gurpil_on_enter_handlers,
    .on_event_handlers = gurpil_on_event_handlers,
    .on_exit_handlers = gurpil_on_exit_handlers,
    .scene_num = GurpilSceneNum,
};
