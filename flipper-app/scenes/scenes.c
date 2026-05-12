#include "scenes.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static const AppSceneOnEnterCallback bitraw_scene_on_enter_handlers[] = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static const AppSceneOnEventCallback bitraw_scene_on_event_handlers[] = {
#include "scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static const AppSceneOnExitCallback bitraw_scene_on_exit_handlers[] = {
#include "scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers bitraw_scene_handlers = {
    .on_enter_handlers = bitraw_scene_on_enter_handlers,
    .on_event_handlers = bitraw_scene_on_event_handlers,
    .on_exit_handlers = bitraw_scene_on_exit_handlers,
    .scene_num = BitrawSceneCount,
};
