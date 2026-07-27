#pragma once

#include <gui/canvas.h>

#include <stdbool.h>

static inline void morse_flipper_draw_tx_history_divider_geometry(
    Canvas* canvas,
    bool left_hint) {
    if(canvas == NULL) return;
    canvas_draw_line(canvas, 0, 34, left_hint ? 119 : 127, 34);
    if(!left_hint) return;

    canvas_draw_box(canvas, 124, 34, 1, 1);
    canvas_draw_box(canvas, 125, 33, 1, 3);
    canvas_draw_box(canvas, 126, 32, 1, 5);
    canvas_draw_box(canvas, 127, 31, 1, 7);
}
