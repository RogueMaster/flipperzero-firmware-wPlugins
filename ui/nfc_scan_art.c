#include "nfc_scan_art.h"
#include <mtools_icons.h>

void mtools_scan_draw_flipper(Canvas* canvas, int x, int y) {
    canvas_draw_icon(canvas, x, y, &I_NFC_manual_60x50);
    /* The stock icon includes a downward arrow and card; keep its Flipper body. */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x + 3, y + 26, 57, 21);
    canvas_set_color(canvas, ColorBlack);
}

void mtools_scan_draw_arrow(Canvas* canvas, int x, int y, bool horizontal) {
    if(horizontal) {
        canvas_draw_box(canvas, x, y + 3, 5, 4);
        canvas_draw_line(canvas, x + 5, y, x + 5, y + 10);
        canvas_draw_line(canvas, x + 6, y + 1, x + 6, y + 9);
        canvas_draw_line(canvas, x + 7, y + 2, x + 7, y + 8);
        canvas_draw_line(canvas, x + 8, y + 3, x + 8, y + 7);
        canvas_draw_line(canvas, x + 9, y + 4, x + 9, y + 6);
        canvas_draw_dot(canvas, x + 10, y + 5);
    } else {
        canvas_draw_box(canvas, x + 3, y, 5, 4);
        canvas_draw_line(canvas, x, y + 4, x + 10, y + 4);
        canvas_draw_line(canvas, x + 1, y + 5, x + 9, y + 5);
        canvas_draw_line(canvas, x + 2, y + 6, x + 8, y + 6);
        canvas_draw_line(canvas, x + 3, y + 7, x + 7, y + 7);
        canvas_draw_line(canvas, x + 4, y + 8, x + 6, y + 8);
        canvas_draw_dot(canvas, x + 5, y + 9);
    }
}

void mtools_scan_draw_fob(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y + 5, x + 2, y + 3);
    canvas_draw_line(canvas, x + 2, y + 3, x + 5, y + 2);
    canvas_draw_line(canvas, x + 5, y + 2, x + 11, y + 1);
    canvas_draw_line(canvas, x + 11, y + 1, x + 17, y);
    canvas_draw_line(canvas, x + 17, y, x + 20, y);
    canvas_draw_line(canvas, x + 20, y, x + 23, y + 2);
    canvas_draw_line(canvas, x + 23, y + 2, x + 25, y + 4);
    canvas_draw_line(canvas, x + 25, y + 4, x + 25, y + 8);
    canvas_draw_line(canvas, x + 25, y + 8, x + 23, y + 10);
    canvas_draw_line(canvas, x + 23, y + 10, x + 20, y + 12);
    canvas_draw_line(canvas, x + 20, y + 12, x + 17, y + 12);
    canvas_draw_line(canvas, x + 17, y + 12, x + 11, y + 11);
    canvas_draw_line(canvas, x + 11, y + 11, x + 5, y + 10);
    canvas_draw_line(canvas, x + 5, y + 10, x + 2, y + 9);
    canvas_draw_line(canvas, x + 2, y + 9, x, y + 7);
    canvas_draw_line(canvas, x, y + 7, x, y + 5);
    canvas_draw_circle(canvas, x + 19, y + 6, 4);
    canvas_draw_circle(canvas, x + 5, y + 6, 2);
}

void mtools_scan_draw_card(Canvas* canvas, int x, int y) {
    canvas_draw_rframe(canvas, x, y, 21, 13, 2);
    canvas_draw_line(canvas, x + 2, y + 3, x + 18, y + 3);
    canvas_draw_line(canvas, x + 2, y + 4, x + 18, y + 4);
}
