// Copyright (c) 2024 Aaron Janeiro Stone
// Licensed under the MIT license.
// See the LICENSE.txt file in the project root for details.

#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>

#include <fencing_testbox_icons.h>

#define TAG "FencingTestbox"

typedef struct FencingTestboxApp FencingTestboxApp;

#define GREEN_PIN gpio_ext_pc3
#define RED_PIN   gpio_ext_pb2

#define TICK_WORK 100 // Number of ticks between gpio checks

// Scene enumeration (1 to 1 with view ids)
typedef enum {
    FencingTestboxSceneMainMenu,
    FencingTestboxSceneWiring,
    FencingTestboxSceneTestbox,
    FenceTestboxSceneUNUSED,
} FencingTestboxScene;

typedef struct {
    bool green_emitting;
    bool red_emitting;
} LightState;

// App context shared across scenes
struct FencingTestboxApp {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    Popup* wiring_popup;
    Widget* widget;
    bool gpio_initialized;
    LightState light_state;
};

// Main menu
void main_menu_scene_on_enter(void* context);
void main_menu_scene_on_exit(void* context);
void main_menu_scene_callback(void* context, uint32_t index);
bool main_menu_scene_on_event(void* context, SceneManagerEvent index);

// Wiring diagram
void wiring_scene_on_enter(void* context);
void wiring_scene_on_exit(void* context);
bool wiring_scene_on_event(void* context, SceneManagerEvent index);

// Testbox
void testbox_scene_on_enter(void* context);
void testbox_scene_on_exit(void* context);
void testbox_scene_on_tick(void* context);
bool testbox_scene_on_event(void* context, SceneManagerEvent index);
