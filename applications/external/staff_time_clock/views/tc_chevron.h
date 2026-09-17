// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// A minimal "V" chevron, hinting that Left/Right does something on this
// screen. Drawn at the screen edges instead of on-screen button labels, and
// shared by every view that offers a Left/Right technology picker.
// =============================================================================

#include <gui/canvas.h>

// x is the screen edge (small for the left chevron, close to 128 for the
// right one - each chevron is 4px wide); y is its vertical center.
static inline void tc_draw_chevron_left(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x + 4, y - 4, x, y);
    canvas_draw_line(canvas, x, y, x + 4, y + 4);
}

static inline void tc_draw_chevron_right(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y - 4, x + 4, y);
    canvas_draw_line(canvas, x + 4, y, x, y + 4);
}
