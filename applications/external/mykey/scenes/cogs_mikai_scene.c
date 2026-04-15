#include "../cogs_mikai.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (* const cogs_mikai_scene_on_enter_handlers[])(void*) = {
#include "cogs_mikai_scene_config.c"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (* const cogs_mikai_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "cogs_mikai_scene_config.c"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (* const cogs_mikai_scene_on_exit_handlers[])(void*) = {
#include "cogs_mikai_scene_config.c"
};
#undef ADD_SCENE

const SceneManagerHandlers cogs_mikai_scene_handlers = {
    .on_enter_handlers = cogs_mikai_scene_on_enter_handlers,
    .on_event_handlers = cogs_mikai_scene_on_event_handlers,
    .on_exit_handlers = cogs_mikai_scene_on_exit_handlers,
    .scene_num = COGSMyKaiSceneCount,
};
