#pragma once

#include "ui_internal.h"

FrItem* item_at(FrGame* game, uint8_t x, uint8_t y);
void draw_text_center(Canvas* canvas, int y, const char* text);
void draw_status(Canvas* canvas, const FrGame* game);
void draw_glyph(Canvas* canvas, uint8_t sx, uint8_t sy, char glyph);
void draw_map(Canvas* canvas, AppContext* app);
void draw_log(Canvas* canvas, const FrGame* game);
void draw_death_log(Canvas* canvas, const FrGame* game);
void draw_menu(Canvas* canvas, AppContext* app);
void draw_callback(Canvas* canvas, void* ctx);
