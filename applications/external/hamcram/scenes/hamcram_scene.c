#include "hamcram_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const hamcram_on_enter_handlers[])(void*) = {
#include "hamcram_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const hamcram_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "hamcram_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const hamcram_on_exit_handlers[])(void* context) = {
#include "hamcram_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers hamcram_scene_handlers = {
    .on_enter_handlers = hamcram_on_enter_handlers,
    .on_event_handlers = hamcram_on_event_handlers,
    .on_exit_handlers = hamcram_on_exit_handlers,
    .scene_num = HamCramSceneNum,
};
