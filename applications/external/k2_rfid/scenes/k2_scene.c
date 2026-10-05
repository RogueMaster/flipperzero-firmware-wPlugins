#include "k2_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static const AppSceneOnEnterCallback k2_on_enter_handlers[] = {
#include "k2_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static const AppSceneOnEventCallback k2_on_event_handlers[] = {
#include "k2_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static const AppSceneOnExitCallback k2_on_exit_handlers[] = {
#include "k2_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers k2_scene_handlers = {
    .on_enter_handlers = k2_on_enter_handlers,
    .on_event_handlers = k2_on_event_handlers,
    .on_exit_handlers = k2_on_exit_handlers,
    .scene_num = K2SceneCount,
};
