#include "game_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const game_on_enter_handlers[])(void*) = {
#include "game_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const game_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "game_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const game_on_exit_handlers[])(void* context) = {
#include "game_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers game_scene_handlers = {
    .on_enter_handlers = game_on_enter_handlers,
    .on_event_handlers = game_on_event_handlers,
    .on_exit_handlers = game_on_exit_handlers,
    .scene_num = GameSceneNum,
};
