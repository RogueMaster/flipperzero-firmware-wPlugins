#pragma once

#include "canvas.h"
#include <string.h>

static inline void elements_button_left(Canvas* canvas, const char* text) {
    (void)canvas;
    (void)text;
}

static inline void elements_button_center(Canvas* canvas, const char* text) {
    canvas->center_buttons++;
    canvas->center_button_font = canvas->current_font;
    strncpy(canvas->center_button_text, text, sizeof(canvas->center_button_text) - 1U);
    canvas_draw_str(canvas, (128 - (int32_t)canvas_string_width(canvas, text)) / 2, 63, text);
}
