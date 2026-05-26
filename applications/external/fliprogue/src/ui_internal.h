#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_logic.h"
#include "score_data.h"
#include "ui_help.h"
#include "ui_layout.h"

#define TILE_PX 6
#define SCREEN_W 128
#define SCREEN_H 64
#define MAP_Y 9
#define APP_VIEW_W 21
#define APP_VIEW_H 8
#define LOG_Y 56
#define TITLE_MENU_COUNT 4
#define MENU_COUNT 5
#define INV_TAB_COUNT 5
#define INV_VISIBLE_ROWS 4
#define SCORE_VISIBLE_ROWS 2
#define HELP_VISIBLE_ROWS 4
#define HELP_FRAME_Y 15
#define HELP_FRAME_H 39
#define HELP_ROW_Y 25
#define HELP_ROW_STEP 8
#define HOLD_REPEAT_MS 150
#define DEW_PHASE_TICKS 3
#define PARALYSIS_TICK_MS 250

typedef enum {
    UI_TITLE = 0,
    UI_CLASS_SELECT,
    UI_SCORES,
    UI_HELP_MENU,
    UI_HELP_PAGE,
    UI_PLAY,
    UI_MENU,
    UI_INVENTORY,
    UI_LOOK,
    UI_TARGET,
    UI_MONSTER_CARD,
    UI_ITEM_CHOICE,
    UI_CHEST_CHOICE,
    UI_IDENTIFY_PICK,
    UI_PERK_CHOICE,
    UI_PERK_DETAIL,
} UiScreen;

typedef struct {
    FrGame* game;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    FuriMutex* mutex;
    UiScreen screen;
    uint8_t selection;
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t target_index;
    uint8_t target_action;
    uint8_t item_index;
    uint8_t camera_x;
    uint8_t camera_y;
    bool camera_valid;
    uint8_t inv_tab;
    uint8_t scroll;
    uint8_t help_topic;
    uint8_t perk_choice;
    bool item_choice_from_pickup;
    uint32_t run_counter;
    ScoreEntry scores[SCORE_CAP];
    uint8_t score_count;
    bool score_recorded;
    bool fx_active;
    uint8_t fx_x;
    uint8_t fx_y;
    uint8_t fx_tx;
    uint8_t fx_ty;
    int8_t fx_dx;
    int8_t fx_dy;
    char fx_glyph;
    uint8_t flash_x;
    uint8_t flash_y;
    uint8_t flash_ticks;
    bool hp_low;
    uint8_t led_state;
    bool hold_blocked;
    uint32_t next_hold_tick;
    uint8_t dew_x;
    uint8_t dew_y;
    uint8_t dew_phase;
    uint8_t dew_ticks;
    uint8_t terrain_anim_tick;
    uint32_t paralysis_next_tick;
    bool sound_enabled;
} AppContext;

typedef struct {
    uint8_t hp;
    uint8_t level;
    bool low_hp;
} FeedbackBefore;
