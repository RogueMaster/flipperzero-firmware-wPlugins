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
        // The two arcs sit ADJACENT. An earlier version left a blank row between
        // them as well as before the dot, and at this size that read as three
        // loose fragments rather than one mark. Only the dot keeps its gap: it is
        // the device, and the arcs are the signal leaving it.
        //
        //   .#####.
        //   #.....#
        //   ..###..
        //   .#...#.
        //   .......
        //   ...#...
        canvas_draw_line(canvas, x + 1, y, x + 5, y);
        canvas_draw_dot(canvas, x, y + 1);
        canvas_draw_dot(canvas, x + 6, y + 1);
        canvas_draw_line(canvas, x + 2, y + 2, x + 4, y + 2);
        canvas_draw_dot(canvas, x + 1, y + 3);
        canvas_draw_dot(canvas, x + 5, y + 3);
        canvas_draw_dot(canvas, x + 3, y + 5);
    }
}

void ui_icon_screen(Canvas* canvas, int x, int y, UiScreenIcon which) {
    switch(which) {
    case UiIconCamera:
        // CCTV in side profile, body TAPERED toward the lens, on a stand.
        //
        // Axis-aligned on purpose. The project logo's 45-degree camera was tried
        // first and was an unreadable blob by 16 px: a rotated edge is entirely
        // staircase, so nothing is left for the shape. Rotation is what these
        // sizes cannot afford, not detail.
        //
        // The taper is load-bearing too. An untapered slab read as a hammer; a
        // camera has to point somewhere for the eye to call it a camera.
        canvas_draw_line(canvas, x + 1, y + 1, x + 6, y + 1);
        canvas_draw_line(canvas, x + 1, y + 2, x + 8, y + 2);
        canvas_draw_line(canvas, x + 1, y + 3, x + 9, y + 3);
        canvas_draw_line(canvas, x + 1, y + 4, x + 8, y + 4);
        canvas_draw_line(canvas, x + 4, y + 5, x + 4, y + 7);
        canvas_draw_line(canvas, x + 2, y + 8, x + 6, y + 8);
        break;
    case UiIconShield:
        // Solid, not outlined: at 9 px an outline leaves an interior too small to
        // read as anything, and the silhouette is what carries "shield".
        canvas_draw_box(canvas, x + 1, y, 9, 5);
        canvas_draw_line(canvas, x + 2, y + 5, x + 8, y + 5);
        canvas_draw_line(canvas, x + 3, y + 6, x + 7, y + 6);
        canvas_draw_line(canvas, x + 4, y + 7, x + 6, y + 7);
        canvas_draw_dot(canvas, x + 5, y + 8);
        break;
    case UiIconCrosshair:
        // Ring plus cardinal ticks. The ticks are what stop it reading as a plain
        // circle at a glance.
        canvas_draw_line(canvas, x + 3, y + 1, x + 7, y + 1);
        canvas_draw_line(canvas, x + 3, y + 7, x + 7, y + 7);
        canvas_draw_line(canvas, x + 1, y + 3, x + 1, y + 5);
        canvas_draw_line(canvas, x + 9, y + 3, x + 9, y + 5);
        canvas_draw_dot(canvas, x + 2, y + 2);
        canvas_draw_dot(canvas, x + 8, y + 2);
        canvas_draw_dot(canvas, x + 2, y + 6);
        canvas_draw_dot(canvas, x + 8, y + 6);
        canvas_draw_line(canvas, x + 5, y, x + 5, y + 2);
        canvas_draw_line(canvas, x + 5, y + 6, x + 5, y + 8);
        canvas_draw_line(canvas, x, y + 4, x + 2, y + 4);
        canvas_draw_line(canvas, x + 8, y + 4, x + 10, y + 4);
        break;
    case UiIconPage:
        // Sheet with two text rules. OUTLINED specifically so it cannot be
        // confused with the solid shield at a glance.
        canvas_draw_line(canvas, x + 1, y, x + 7, y);
        canvas_draw_line(canvas, x + 1, y, x + 1, y + 8);
        canvas_draw_line(canvas, x + 1, y + 8, x + 8, y + 8);
        canvas_draw_line(canvas, x + 8, y + 3, x + 8, y + 8);
        canvas_draw_line(canvas, x + 7, y, x + 8, y + 1);
        canvas_draw_line(canvas, x + 8, y + 1, x + 8, y + 3);
        canvas_draw_line(canvas, x + 3, y + 3, x + 6, y + 3);
        canvas_draw_line(canvas, x + 3, y + 5, x + 6, y + 5);
        break;
    case UiIconChip:
    default:
        // IC package with legs on both sides.
        canvas_draw_frame(canvas, x + 2, y + 1, 7, 7);
        canvas_draw_box(canvas, x + 4, y + 3, 3, 3);
        for(int i = 0; i < 3; i++) {
            int ly = y + 2 + i * 2;
            canvas_draw_line(canvas, x, ly, x + 1, ly);
            canvas_draw_line(canvas, x + 9, ly, x + 10, ly);
        }
        break;
    }
}

int ui_title_bar_icon(Canvas* canvas, UiScreenIcon icon, const char* title, const char* right) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, UI_TITLE_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    ui_icon_screen(canvas, 2, 2, icon);
    canvas_set_font(canvas, FontPrimary);
    if(title) canvas_draw_str(canvas, 2 + UI_SCREEN_ICON_W + 3, 10, title);
    if(right) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, right);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    return UI_TITLE_BAR_H + 2;
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
