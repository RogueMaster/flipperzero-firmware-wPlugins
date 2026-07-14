#include "app_scene_functions.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const tesla_fsd_on_enter_handlers[])(void*) = {
#include "app_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const tesla_fsd_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "app_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const tesla_fsd_on_exit_handlers[])(void* context) = {
#include "app_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers tesla_fsd_scene_handlers = {
    .on_enter_handlers = tesla_fsd_on_enter_handlers,
    .on_event_handlers = tesla_fsd_on_event_handlers,
    .on_exit_handlers = tesla_fsd_on_exit_handlers,
    .scene_num = tesla_fsd_scene_count,
};
