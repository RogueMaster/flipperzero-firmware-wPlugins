#pragma once

#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include "scenes/import.h"

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    void** allocated_scenes;
} App;

/**
 * Enum for scenes.
 */
typedef enum {
#define SCENE_ACTION(scene) scene,
#include "scenes/list.h"
#undef SCENE_ACTION

    AppSceneNum, // This should be the last element in the enumeration.
} AppViews;

/**
 * Header definition for handler.c
 */
bool scene_handler_event_forwarder(void* context, uint32_t event_id);
bool scene_handler_navigation_forwarder(void* context);
