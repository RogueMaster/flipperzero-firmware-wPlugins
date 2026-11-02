#include "fn_scene.h"

/* Таблицы обработчиков собираются из того же списка, что и enum сцен. */

static const AppSceneOnEnterCallback fn_scene_on_enter_handlers[] = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
#include "fn_scene_config.h"
#undef ADD_SCENE
};

static const AppSceneOnEventCallback fn_scene_on_event_handlers[] = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
#include "fn_scene_config.h"
#undef ADD_SCENE
};

static const AppSceneOnExitCallback fn_scene_on_exit_handlers[] = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
#include "fn_scene_config.h"
#undef ADD_SCENE
};

const SceneManagerHandlers fn_scene_handlers = {
    .on_enter_handlers = fn_scene_on_enter_handlers,
    .on_event_handlers = fn_scene_on_event_handlers,
    .on_exit_handlers = fn_scene_on_exit_handlers,
    .scene_num = FNAppSceneNum,
};
