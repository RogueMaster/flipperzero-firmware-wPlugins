#include "../glitch_trigger_i.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const glitch_scene_on_enter_handlers[])(void*) = {
#include "glitch_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const glitch_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "glitch_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const glitch_scene_on_exit_handlers[])(void* context) = {
#include "glitch_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers glitch_scene_handlers = {
    .on_enter_handlers = glitch_scene_on_enter_handlers,
    .on_event_handlers = glitch_scene_on_event_handlers,
    .on_exit_handlers = glitch_scene_on_exit_handlers,
    .scene_num = GlitchSceneNum,
};
