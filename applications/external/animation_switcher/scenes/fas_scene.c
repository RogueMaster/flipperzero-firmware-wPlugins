#include "fas_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const fas_on_enter_handlers[])(void*) = {
#include "fas_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const fas_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "fas_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const fas_on_exit_handlers[])(void*) = {
#include "fas_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers fas_scene_handlers = {
    .on_enter_handlers = fas_on_enter_handlers,
    .on_event_handlers = fas_on_event_handlers,
    .on_exit_handlers = fas_on_exit_handlers,
    .scene_num = FasSceneCount,
};
