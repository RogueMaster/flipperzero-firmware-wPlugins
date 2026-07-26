#pragma once

#include <gui/scene_manager.h>

// Settings
#define SCENE_HEADER_POSITION_Y      0
#define SCENE_UI_UPDATE_PERIOD_MS    250
#define SCENE_FILE_BROWSER_BASE_PATH "/ext"

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) NfcShareScene##id,
typedef enum {
#include "nfc_share_scene_config.h"
    NfcShareSceneNum,
} NfcShareScene;
#undef ADD_SCENE

extern const SceneManagerHandlers nfc_share_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "nfc_share_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "nfc_share_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "nfc_share_scene_config.h"
#undef ADD_SCENE
