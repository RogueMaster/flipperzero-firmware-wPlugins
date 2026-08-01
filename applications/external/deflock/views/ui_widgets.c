// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "ui_widgets.h"

#include "../helpers/report_fmt.h"

#include <string.h>

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

void ui_icon_radio(Canvas* canvas, int x, int y, bool ble) {
    if(ble) {
        // Bluetooth rune, 5 wide inside the 7-square: a vertical stroke with the
        // two triangles crossing it. Five strokes, in the caller's colour.
        canvas_draw_line(canvas, x + 3, y, x + 3, y + 6);
        canvas_draw_line(canvas, x + 3, y, x + 5, y + 2);
        canvas_draw_line(canvas, x + 5, y + 2, x + 1, y + 4);
        canvas_draw_line(canvas, x + 3, y + 6, x + 5, y + 4);
        canvas_draw_line(canvas, x + 5, y + 4, x + 1, y + 2);
    } else {
        // Wi-Fi: two arcs over a dot.
        //
        // The arcs are FLAT (a horizontal run plus two dropped end pixels), not
        // chevrons. Chevrons were tried first and rendered as a closed diamond on
        // hardware: the outer chevron's descending arms met the inner one's and
        // the whole thing read as a rhombus, indistinguishable from a generic
        // marker. A blank row between each arc keeps them from merging at this
        // size.
        //
        //   .#####.
        //   #.....#
        //   .......
        //   ..###..
        //   .#...#.
        //   .......
        //   ...#...
        canvas_draw_line(canvas, x + 1, y, x + 5, y);
        canvas_draw_dot(canvas, x, y + 1);
        canvas_draw_dot(canvas, x + 6, y + 1);
        canvas_draw_line(canvas, x + 2, y + 3, x + 4, y + 3);
        canvas_draw_dot(canvas, x + 1, y + 4);
        canvas_draw_dot(canvas, x + 5, y + 4);
        canvas_draw_dot(canvas, x + 3, y + 6);
    }
}

void ui_draw_str_fit(Canvas* canvas, int x, int baseline, const char* s, int max_x) {
    int avail = max_x - x;
    if(avail <= 0) return;
    if(canvas_string_width(canvas, s) <= avail) {
        canvas_draw_str(canvas, x, baseline, s);
        return;
    }
    char probe[52];
    size_t n = strlen(s);
    if(n > sizeof(probe) - 3) n = sizeof(probe) - 3;
    while(n > 0) {
        n--;
        memcpy(probe, s, n);
        probe[n] = '.';
        probe[n + 1] = '.';
        probe[n + 2] = '\0';
        if(canvas_string_width(canvas, probe) <= avail) break;
    }
    canvas_draw_str(canvas, x, baseline, probe);
}
