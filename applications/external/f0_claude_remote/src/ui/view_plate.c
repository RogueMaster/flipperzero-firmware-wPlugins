#include "view_plate.h"

void view_plate_draw_white_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, str);
    canvas_set_color(canvas, ColorBlack);
}

void view_plate_draw_white_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, x, y, icon);
    canvas_set_color(canvas, ColorBlack);
}

/* Clears first: the recess runs under the plates that lie on it. */
void view_plate_draw_outline(Canvas* canvas, int32_t x, int32_t y, size_t width) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, width, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, width, 11, 2);
}

/* Stops short of the outline's rows, where it would square the plate's corners. */
void view_plate_draw_fill(Canvas* canvas, int32_t x, int32_t y, size_t width) {
    canvas_draw_box(canvas, x, y + 1, width, 9);
}
