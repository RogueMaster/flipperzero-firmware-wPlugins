#include "dnie_reader_scene.h"

// Arrays de punteros a funcion que SceneManager usa para despachar

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const dnie_scene_on_enter_handlers[])(void*) = {
#include "dnie_reader_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const dnie_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "dnie_reader_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const dnie_scene_on_exit_handlers[])(void*) = {
#include "dnie_reader_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers dnie_scene_handlers = {
    .on_enter_handlers = dnie_scene_on_enter_handlers,
    .on_event_handlers = dnie_scene_on_event_handlers,
    .on_exit_handlers = dnie_scene_on_exit_handlers,
    .scene_num = DnieSceneNum,
};
