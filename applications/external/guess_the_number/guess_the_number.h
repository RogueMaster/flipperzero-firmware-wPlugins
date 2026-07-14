#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include "scenes/game_scene.h"
#include "views/game_startscreen.h"
#include "views/game_instructions.h"
#include "views/game_play.h"
#include "helpers/game_storage.h"

#define TAG "GuessTheNumber"

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    GameStartscreen* game_startscreen;
    GameInstructions* game_instructions;
    GamePlay* game_play;
    uint32_t haptic;
    uint32_t speaker;
    uint32_t led;
    uint32_t save_settings;
} GameApp;

typedef enum {
    GameViewIdStartscreen,
    GameViewIdMenu,
    GameViewIdInstructions,
    GameViewIdPlay,
    GameViewIdSettings,
} GameViewId;

typedef enum {
    GameHapticOff,
    GameHapticOn,
} GameHapticState;

typedef enum {
    GameSpeakerOff,
    GameSpeakerOn,
} GameSpeakerState;

typedef enum {
    GameLedOff,
    GameLedOn,
} GameLedState;

typedef enum {
    GameSettingsOff,
    GameSettingsOn,
} GameSettingsStoreState;
