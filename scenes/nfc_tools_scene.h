#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) NfcToolsSceneId##id,
typedef enum {
#include "nfc_tools_scene_config.h"
    NfcToolsSceneNum,
} NfcToolsSceneId;
#undef ADD_SCENE

extern const SceneManagerHandlers nfc_tools_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                              \
    void prefix##_scene_##name##_on_enter(void*);                                \
    bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent);             \
    void prefix##_scene_##name##_on_exit(void*);
#include "nfc_tools_scene_config.h"
#undef ADD_SCENE
