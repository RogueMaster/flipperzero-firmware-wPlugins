#pragma once

#include <gui/canvas.h>
#include <math.h>

// Draws a 5x7 "±" character sitting on baseline y like a digit. Returns its width.
static inline int32_t nfc_rfid_detector_draw_pm(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_line(canvas, x + 2, y - 7, x + 2, y - 3);
    canvas_draw_line(canvas, x, y - 5, x + 4, y - 5);
    canvas_draw_line(canvas, x, y - 1, x + 4, y - 1);
    return 6;
}

// Decimal places for a figure quoted with its ±
static inline int nfc_rfid_detector_draw_decimals(double uncertainty, int max_decimals) {
    if(uncertainty <= 0) return max_decimals;
    int exponent = (int)floor(log10(uncertainty));
    int decimals = -exponent;
    if(uncertainty / pow(10, exponent) < 2) decimals++;
    if(decimals < 0) return 0;
    return decimals > max_decimals ? max_decimals : decimals;
}
