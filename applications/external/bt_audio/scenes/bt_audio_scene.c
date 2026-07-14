#include "../bt_audio.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const bt_audio_on_enter_handlers[])(void*) = {
#include "bt_audio_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const bt_audio_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "bt_audio_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const bt_audio_on_exit_handlers[])(void* context) = {
#include "bt_audio_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration
const SceneManagerHandlers bt_audio_scene_handlers = {
    .on_enter_handlers = bt_audio_on_enter_handlers,
    .on_event_handlers = bt_audio_on_event_handlers,
    .on_exit_handlers = bt_audio_on_exit_handlers,
    .scene_num = BtAudioSceneNum,
};
