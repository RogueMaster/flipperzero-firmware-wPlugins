#include "marauder_gui_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const marauder_gui_scene_on_enter_handlers[])(void*) = {
#include "marauder_gui_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const marauder_gui_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "marauder_gui_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const marauder_gui_scene_on_exit_handlers[])(void* context) = {
#include "marauder_gui_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers marauder_gui_scene_handlers = {
    .on_enter_handlers = marauder_gui_scene_on_enter_handlers,
    .on_event_handlers = marauder_gui_scene_on_event_handlers,
    .on_exit_handlers = marauder_gui_scene_on_exit_handlers,
    .scene_num = MarauderGuiSceneNum,
};
