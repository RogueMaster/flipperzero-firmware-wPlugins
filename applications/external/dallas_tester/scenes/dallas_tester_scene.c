#include "dallas_tester_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const dallas_tester_on_enter_handlers[])(void*) = {
#include "dallas_tester_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const dallas_tester_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "dallas_tester_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const dallas_tester_on_exit_handlers[])(void* context) = {
#include "dallas_tester_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers dallas_tester_scene_handlers = {
    .on_enter_handlers = dallas_tester_on_enter_handlers,
    .on_event_handlers = dallas_tester_on_event_handlers,
    .on_exit_handlers = dallas_tester_on_exit_handlers,
    .scene_num = DallasTesterSceneNum,
};
