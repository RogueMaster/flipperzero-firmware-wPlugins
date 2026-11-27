#pragma once

#include <gui/canvas.h>

#include <stdbool.h>

static inline void
    morse_flipper_draw_tx_history_divider_geometry_at(Canvas* canvas, bool left_hint, int32_t y) {
    if(canvas == NULL) return;
    canvas_draw_line(canvas, 0, y, left_hint ? 119 : 127, y);
    if(!left_hint) return;

    canvas_draw_box(canvas, 124, y, 1, 1);
    canvas_draw_box(canvas, 125, y - 1, 1, 3);
    canvas_draw_box(canvas, 126, y - 2, 1, 5);
    canvas_draw_box(canvas, 127, y - 3, 1, 7);
}

static inline void morse_flipper_draw_tx_history_divider_geometry(Canvas* canvas, bool left_hint) {
    morse_flipper_draw_tx_history_divider_geometry_at(canvas, left_hint, 34);
}
