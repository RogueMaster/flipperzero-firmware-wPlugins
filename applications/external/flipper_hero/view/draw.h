#include <gui/icon.h>
#include <stdio.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include "../helpers/stratagem_data.h"
#include "../data/data.h"
#include "arrows.h"
#pragma once

// For adjusting pause menu
#define PAUSE_MENU_MARGIN_X  8
#define PAUSE_MENU_MARGIN_Y  4
#define PAUSE_MENU_PADDING_X 2
#define PAUSE_MENU_PADDING_Y 4
#define MENU_OPTION_SPACE_Y  12

// These are for name printing and the marquee effect.
#define MARQUEE_SPEED    50 // Amount of ticks between marquee movements
#define MARQUEE_DELAY    50 // Amount of ticks before the marquee starts moving (with new stratagem)
#define MARQUEE_DISTANCE 10 // Amount of pixels marquee moves at a time
#define MARQUEE_BASE_PADDING \
    10 // The minimum space (in pixels) between the start and end of marquee text

// For adjusting the stratagem name display
#define NAME_POS_Y     35
#define NAME_PADDING_X 2
#define NAME_PADDING_Y 1

void update_marquee_data(PluginState* plugin_state);
void init_marquee_data(Canvas* const canvas, PluginState* plugin_state);
void draw_round_score(PluginState* plugin_state, Canvas* const canvas);
void draw_pre_round(PluginState* plugin_state, Canvas* const canvas);
void draw_start_screen(Canvas* const canvas);
void draw_game_over_screen(Canvas* const canvas, PluginState* plugin_state);
void draw_name(Canvas* const canvas, PluginState* plugin_state);
void draw_game_ui(Canvas* const canvas, int round, int score);
void draw_icon(Canvas* const canvas, int8_t pos_x, int8_t pos_y, int8_t icon_index, bool large_icon);
void draw_queue(Canvas* const canvas, PluginState* plugin_state);
void draw_arrows(Canvas* const canvas, PluginState* plugin_state);
void draw_progress_box(Canvas* const canvas, int timer);
void draw_pause_menu(Canvas* const canvas, PluginState* plugin_state);
