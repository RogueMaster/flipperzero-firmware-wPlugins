#pragma once

#include <gui/canvas.h>

#include <stdbool.h>
#include <stddef.h>

void uhf_view_draw_right_text(Canvas* canvas, int y, const char* text);
void uhf_view_draw_right_fraction(Canvas* canvas, int y, size_t current, size_t total);
void uhf_view_draw_fixed_side_button(Canvas* canvas, const char* text, bool right);
void uhf_view_draw_fixed_center_button(Canvas* canvas, const char* text);
