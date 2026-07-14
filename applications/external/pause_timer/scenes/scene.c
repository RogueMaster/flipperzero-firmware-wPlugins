#include "scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const pause_timer_on_enter_handlers[])(void*) = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const pause_timer_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const pause_timer_on_exit_handlers[])(void* context) = {
#include "scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers pause_timer_scene_handlers = {
    .on_enter_handlers = pause_timer_on_enter_handlers,
    .on_event_handlers = pause_timer_on_event_handlers,
    .on_exit_handlers = pause_timer_on_exit_handlers,
    .scene_num = PauseTimerSceneNum,
};
