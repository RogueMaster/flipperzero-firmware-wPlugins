#include "../rosetta_i.h"

// Generate on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const rosetta_scene_on_enter_handlers[])(void*) = {
#include "rosetta_scene_config.h"
};
#undef ADD_SCENE

// Generate on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const rosetta_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "rosetta_scene_config.h"
};
#undef ADD_SCENE

// Generate on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const rosetta_scene_on_exit_handlers[])(void* context) = {
#include "rosetta_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers rosetta_scene_handlers = {
    .on_enter_handlers = rosetta_scene_on_enter_handlers,
    .on_event_handlers = rosetta_scene_on_event_handlers,
    .on_exit_handlers = rosetta_scene_on_exit_handlers,
    .scene_num = RosettaSceneNum,
};
