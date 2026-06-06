#include "nfc_tools_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const nfc_tools_on_enter_handlers[])(void*) = {
#include "nfc_tools_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const nfc_tools_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "nfc_tools_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const nfc_tools_on_exit_handlers[])(void* context) = {
#include "nfc_tools_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers nfc_tools_scene_handlers = {
    .on_enter_handlers = nfc_tools_on_enter_handlers,
    .on_event_handlers = nfc_tools_on_event_handlers,
    .on_exit_handlers = nfc_tools_on_exit_handlers,
    .scene_num = NfcToolsSceneNum,
};
