#pragma once

#include "p1x_network_defender.h"

// Drawing functions
void draw_callback(Canvas* canvas, void* ctx);
void draw_title_screen(Canvas* canvas);
void draw_menu_screen(Canvas* canvas, int selection);
void draw_help_screen(Canvas* canvas, int page);
void draw_upcoming_packets(Canvas* canvas, GameState* state);

// Input handling
void input_callback(InputEvent* input, void* ctx);
