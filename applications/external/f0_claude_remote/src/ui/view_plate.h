#pragma once

#include <gui/canvas.h>

void view_plate_draw_white_str(Canvas* canvas, int32_t x, int32_t y, const char* str);

void view_plate_draw_white_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon);

void view_plate_draw_outline(Canvas* canvas, int32_t x, int32_t y, size_t width);

void view_plate_draw_fill(Canvas* canvas, int32_t x, int32_t y, size_t width);
