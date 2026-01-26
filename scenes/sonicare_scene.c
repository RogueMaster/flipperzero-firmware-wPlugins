#include "sonicare_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const sonicare_on_enter_handlers[])(void*) = {
#include "sonicare_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const sonicare_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "sonicare_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const sonicare_on_exit_handlers[])(void* context) = {
#include "sonicare_scene_config.h"
};
#undef ADD_SCENE

// Initialise scene handlers configuration structure
const SceneManagerHandlers sonicare_scene_handlers = {
    .on_enter_handlers = sonicare_on_enter_handlers,
    .on_event_handlers = sonicare_on_event_handlers,
    .on_exit_handlers = sonicare_on_exit_handlers,
    .scene_num = SonicareSceneNum,
};
