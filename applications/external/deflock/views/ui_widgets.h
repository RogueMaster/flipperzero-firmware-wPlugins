// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/gui.h>

// Shared "Guardian HUD" visual language. Small, dependency-free canvas helpers so
// every screen frames itself the same way. (Owned by HEAD_DEV per the 2026
// session; PIXEL/GLYPH build their screens on top of these.)

#define UI_TITLE_BAR_H 13

/**
 * Inverted title bar: a filled black bar across the top (0,0,128,13) with `title`
 * in white FontPrimary at the left and, when non-NULL, `right` right-aligned in
 * white FontSecondary. Leaves the canvas color black + font Secondary on return.
 * @return y of the first content row below the bar.
 */
int ui_title_bar(Canvas* canvas, const char* title, const char* right);

/** Map an RSSI (dBm; 0 = unknown) to a strength level: -1 unknown, else 0..4. */
int ui_signal_level(int rssi);

/**
 * 4-segment signal-strength bars from an RSSI (dBm). Occupies a ~11x9 cell with
 * its top-left at (x, y). rssi == 0 draws hollow "unknown" ticks.
 */
void ui_signal_bars(Canvas* canvas, int x, int y, int rssi);

/** Framed horizontal meter at (x,y,w,h), filled left-to-right to pct (0..100). */
void ui_meter(Canvas* canvas, int x, int y, int w, int h, int pct);
