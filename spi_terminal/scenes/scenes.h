#pragma once

#include <gui/scene_manager.h>

#include "../flipper_spi_terminal_app.h"

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) FlipperSPITerminalAppScene##id,
typedef enum {
#include "scenes_config.h"
    FlipperSPITerminalAppSceneNum,
} FlipperSPITerminalAppScene;
#undef ADD_SCENE

extern const SceneManagerHandlers flipper_spi_terminal_scene_handlers;

// Generate scene alloc handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_alloc(FlipperSPITerminalApp* app);
#include "scenes_config.h"
#undef ADD_SCENE

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "scenes_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "scenes_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "scenes_config.h"
#undef ADD_SCENE

// Generate scene free handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_free(FlipperSPITerminalApp* app);
#include "scenes_config.h"
#undef ADD_SCENE

void flipper_spi_terminal_scenes_alloc(FlipperSPITerminalApp* app);
void flipper_spi_terminal_scenes_free(FlipperSPITerminalApp* app);
