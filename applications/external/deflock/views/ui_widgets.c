// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "ui_widgets.h"

#include "../helpers/report_fmt.h"

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
    // Thresholds live in helpers/report_fmt.c so the canvas bars here and the
    // text meter on the detail screen are the same scale by construction.
    return fmt_signal_level(rssi);
}

void ui_signal_bars(Canvas* canvas, int x, int y, int rssi) {
    int level = ui_signal_level(rssi);
    // Draw in the CALLER's current color rather than forcing black. Forcing it
    // made the bars invisible on an inverted (selected) row, so every list fell
    // back to raw "-82dB" text there -- which is why one screen showed two
    // different notations for the same field (GitHub issue #5). Every caller
    // already sets the row color immediately before calling.
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
