#include "miband_nfc_scene.h"

// Generate scene on_enter handlers array
void (*const miband_nfc_on_enter_handlers[])(void*) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE
};

// Generate scene on_event handlers array
bool (*const miband_nfc_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE
};

// Generate scene on_exit handlers array
void (*const miband_nfc_on_exit_handlers[])(void* context) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE
};

// Initialize scene handlers configuration
const SceneManagerHandlers miband_nfc_scene_handlers = {
    .on_enter_handlers = miband_nfc_on_enter_handlers,
    .on_event_handlers = miband_nfc_on_event_handlers,
    .on_exit_handlers = miband_nfc_on_exit_handlers,
    .scene_num = MiBandNfcSceneNum,
};
