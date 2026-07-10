#pragma once

#include <gui/scene_manager.h>

/*
 * Scene table for the app's classic Flipper menu flow: Menu (Submenu) -> Game | HowToPlay |
 * Credits, each a leaf the player Backs out of to return to Menu, and Back at Menu exits the
 * app (SceneManager's own default: an unconsumed Back event pops the current scene, and popping
 * the last scene off the stack reports "no previous scene" to the ViewDispatcher, which then
 * stops the app run loop). Generated via the same ADD_SCENE X-macro idiom the firmware's own
 * scene-driven apps use (see e.g. applications/examples/example_number_input).
 */

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) GurpilScene##id,
typedef enum {
#include "include/scenes/gurpil_scene_config.h"
    GurpilSceneNum,
} GurpilScene;
#undef ADD_SCENE

extern const SceneManagerHandlers gurpil_scene_handlers;

// Generate scene on_enter handler declarations
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void *);
#include "include/scenes/gurpil_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handler declarations
#define ADD_SCENE(prefix, name, id)                                                                \
    bool prefix##_scene_##name##_on_event(void *context, SceneManagerEvent event);
#include "include/scenes/gurpil_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handler declarations
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void *context);
#include "include/scenes/gurpil_scene_config.h"
#undef ADD_SCENE
