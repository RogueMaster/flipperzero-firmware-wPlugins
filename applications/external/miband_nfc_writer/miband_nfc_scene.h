/**
 * @file miband_nfc_scene.h
 * @brief Scene manager declarations and scene ID enumeration
 * 
 * This file declares all scene handler functions and defines the scene ID
 * enumeration using the X-Macro pattern. Each scene has three handlers:
 * - on_enter: Called when entering the scene
 * - on_event: Called to handle events while in the scene
 * - on_exit: Called when leaving the scene
 */

#pragma once

#include <gui/scene_manager.h>

/**
 * Generate scene on_enter handler declarations
 * 
 * Creates declarations like:
 * void miband_nfc_scene_main_menu_on_enter(void*);
 * void miband_nfc_scene_file_select_on_enter(void*);
 * etc.
 */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE

/**
 * Generate scene on_event handler declarations
 * 
 * Creates declarations like:
 * bool miband_nfc_scene_main_menu_on_event(void* context, SceneManagerEvent event);
 * bool miband_nfc_scene_file_select_on_event(void* context, SceneManagerEvent event);
 * etc.
 * 
 * Returns true if the event was consumed, false otherwise.
 */
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE

/**
 * Generate scene on_exit handler declarations
 * 
 * Creates declarations like:
 * void miband_nfc_scene_main_menu_on_exit(void* context);
 * void miband_nfc_scene_file_select_on_exit(void* context);
 * etc.
 */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE

/**
 * Scene handlers structure
 * 
 * This structure contains arrays of function pointers for all scene handlers.
 * It is used by the scene manager to call the appropriate handlers.
 */
extern const SceneManagerHandlers miband_nfc_scene_handlers;

/**
 * Scene ID enumeration
 * 
 * Each scene has a unique ID used by the scene manager to identify
 * and switch between scenes.
 * 
 * Generated from miband_nfc_scene_config.h using the X-Macro pattern.
 * Creates enum values like:
 * - MiBandNfcSceneMainMenu
 * - MiBandNfcSceneFileSelect
 * - MiBandNfcSceneMagicEmulator
 * etc.
 * 
 * MiBandNfcSceneNum is automatically set to the total number of scenes.
 */
typedef enum {
#define ADD_SCENE(prefix, name, id) MiBandNfcScene##id,
#include "miband_nfc_scene_config.h"
#undef ADD_SCENE
    MiBandNfcSceneNum, // Total number of scenes
} MiBandNfcScene;
