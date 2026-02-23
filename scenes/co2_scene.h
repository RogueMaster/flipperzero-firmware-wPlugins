#pragma once

#include <gui/scene_manager.h>

// Generate scene id enum
#define ADD_SCENE(prefix, name, id) Co2Scene##id,
typedef enum {
#include "config/co2_scene_config.h"
    Co2SceneCount,
} Co2Scene;
#undef ADD_SCENE

extern const SceneManagerHandlers co2_scene_handlers;
