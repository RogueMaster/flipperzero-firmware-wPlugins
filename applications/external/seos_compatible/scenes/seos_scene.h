#pragma once

#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <input/input.h>

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) SeosScene##id,
typedef enum {
#include "seos_scene_config.h"
    SeosSceneNum,
} SeosScene;
#undef ADD_SCENE

extern const SceneManagerHandlers seos_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "seos_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "seos_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "seos_scene_config.h"
#undef ADD_SCENE

/* Sends a widget button press on as a custom event.
 *
 * Every scene with buttons wanted exactly this, and seven carried their own
 * copy of it. */
void seos_scene_widget_callback(GuiButtonType result, InputType type, void* context);
