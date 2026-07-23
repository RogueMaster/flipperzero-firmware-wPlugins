#include "../gauntlet_i.h"

// Generate on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const gauntlet_scene_on_enter_handlers[])(void*) = {
#include "gauntlet_scene_config.h"
};
#undef ADD_SCENE

// Generate on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const gauntlet_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "gauntlet_scene_config.h"
};
#undef ADD_SCENE

// Generate on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const gauntlet_scene_on_exit_handlers[])(void* context) = {
#include "gauntlet_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers gauntlet_scene_handlers = {
    .on_enter_handlers = gauntlet_scene_on_enter_handlers,
    .on_event_handlers = gauntlet_scene_on_event_handlers,
    .on_exit_handlers = gauntlet_scene_on_exit_handlers,
    .scene_num = GauntletSceneNum,
};
