#include "trident_scene.h"

// on_enter handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const trident_scene_on_enter_handlers[])(void*) = {
#include "trident_scene_config.h"
};
#undef ADD_SCENE

// on_event handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const trident_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "trident_scene_config.h"
};
#undef ADD_SCENE

// on_exit handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const trident_scene_on_exit_handlers[])(void* context) = {
#include "trident_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers trident_scene_handlers = {
    .on_enter_handlers = trident_scene_on_enter_handlers,
    .on_event_handlers = trident_scene_on_event_handlers,
    .on_exit_handlers = trident_scene_on_exit_handlers,
    .scene_num = TridentSceneNum,
};
