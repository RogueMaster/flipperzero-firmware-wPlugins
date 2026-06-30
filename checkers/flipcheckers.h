#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include "scenes/flipcheckers_scene.h"
#include "views/flipcheckers_startscreen.h"
#include "views/flipcheckers_scene_1.h"

#define FLIPCHECKERS_VERSION "v1.0"

#define TEXT_BUFFER_SIZE 96
#define TEXT_SIZE        (TEXT_BUFFER_SIZE - 1)

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    TextInput* text_input;
    TextBox* text_box;
    FlipCheckersStartscreen* flipcheckers_startscreen;
    FlipCheckersScene1* flipcheckers_scene_1;
    // Settings options
    int haptic;
    int sound;
    int white_mode;
    int black_mode;
    int must_jump;
    // Main menu options
    uint8_t import_game;
    // Text input
    uint8_t input_state;
    char import_game_text[TEXT_BUFFER_SIZE];
    char input_text[TEXT_BUFFER_SIZE];
} FlipCheckers;

typedef enum {
    FlipCheckersViewIdStartscreen,
    FlipCheckersViewIdMenu,
    FlipCheckersViewIdScene1,
    FlipCheckersViewIdSettings,
    FlipCheckersViewIdTextInput,
    FlipCheckersViewIdAbout,
} FlipCheckersViewId;

typedef enum {
    FlipCheckersMustJumpOff = 0,
    FlipCheckersMustJumpOn  = 1,
} FlipCheckersMustJump;

typedef enum {
    FlipCheckersHapticOff,
    FlipCheckersHapticOn,
} FlipCheckersHapticState;

typedef enum {
    FlipCheckersSoundOff,
    FlipCheckersSoundOn,
} FlipCheckersSoundState;

typedef enum {
    FlipCheckersPlayerHuman = 0,
    FlipCheckersPlayerAI1 = 1,
    FlipCheckersPlayerAI2 = 2,
    FlipCheckersPlayerAI3 = 3,
} FlipCheckersPlayerMode;

typedef enum {
    FlipCheckersTextInputDefault,
    FlipCheckersTextInputGame
} FlipCheckersTextInputState;

void flipcheckers_save_settings(FlipCheckers* app);
void flipcheckers_load_settings(FlipCheckers* app);

typedef enum {
    FlipCheckersStatusNone = 0,
    FlipCheckersStatusMovePlayer = 1,
    FlipCheckersStatusMoveAI = 2,
    FlipCheckersStatusMoveUndo = 3,
    FlipCheckersStatusReturn = 10,
    FlipCheckersStatusLoadError = 11,
    FlipCheckersStatusSaveError = 12,
} FlipCheckersStatus;
