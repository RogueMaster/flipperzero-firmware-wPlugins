// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "ui_widgets.h"

int ui_title_bar(Canvas* canvas, const char* title, const char* right) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, UI_TITLE_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, title);
    if(right) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, right);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    return UI_TITLE_BAR_H + 2; // small gap under the bar
}

int ui_signal_level(int rssi) {
    if(rssi == 0) return -1; // unknown
    if(rssi >= -50) return 4;
    if(rssi >= -62) return 3;
    if(rssi >= -74) return 2;
    if(rssi >= -86) return 1;
    return 1; // very weak but present
}

void ui_signal_bars(Canvas* canvas, int x, int y, int rssi) {
    int level = ui_signal_level(rssi);
    canvas_set_color(canvas, ColorBlack);
    // 4 bars, width 2, heights 2/4/6/8, sharing a baseline at y+8.
    for(int i = 0; i < 4; i++) {
        int bh = 2 + i * 2;
        int bx = x + i * 3;
        int by = y + 8 - bh;
        if(level < 0) {
            canvas_draw_frame(canvas, bx, by, 2, bh); // unknown -> hollow ticks
        } else if(i < level) {
            canvas_draw_box(canvas, bx, by, 2, bh); // lit
        } else {
            canvas_draw_dot(canvas, bx, y + 7); // empty -> base dot
        }
    }
}

void ui_meter(Canvas* canvas, int x, int y, int w, int h, int pct) {
    if(pct < 0) pct = 0;
    if(pct > 100) pct = 100;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);
    int fill = ((w - 2) * pct) / 100;
    if(fill > 0) canvas_draw_box(canvas, x + 1, y + 1, fill, h - 2);
}
