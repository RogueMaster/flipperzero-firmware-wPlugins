#include "sigroam_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const sigroam_on_enter_handlers[])(void*) = {
#include "sigroam_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const sigroam_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "sigroam_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const sigroam_on_exit_handlers[])(void* context) = {
#include "sigroam_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers sigroam_scene_handlers = {
    .on_enter_handlers = sigroam_on_enter_handlers,
    .on_event_handlers = sigroam_on_event_handlers,
    .on_exit_handlers = sigroam_on_exit_handlers,
    .scene_num = SigRoamSceneNum,
};
