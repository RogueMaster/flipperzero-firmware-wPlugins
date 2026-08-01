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
 *
 * Drawn in the canvas's CURRENT color, so it renders on an inverted/selected row
 * as long as the caller has set ColorWhite first. It does not set the color
 * itself, and it does not restore one.
 */
void ui_signal_bars(Canvas* canvas, int x, int y, int rssi);

/** Framed horizontal meter at (x,y,w,h), filled left-to-right to pct (0..100). */
void ui_meter(Canvas* canvas, int x, int y, int w, int h, int pct);

/** Side of the square cell ui_icon_radio() draws into. */
#define UI_RADIO_ICON_W 7

/**
 * Which radio a sighting came in on, as a glyph: the Bluetooth rune for BLE,
 * the arcs-over-a-dot mark for Wi-Fi. Occupies a UI_RADIO_ICON_W square with its
 * top-left at (x, y).
 *
 * Requested on issue #5: an OUI or an RSSI tells you nothing about which radio
 * saw the device, and the only other tell is whether the row happens to show an
 * SSID -- which a hidden AP and a probe request also lack. Drawn from canvas
 * primitives rather than a compiled asset: two icons cost ~10 lines here against
 * the RAM budget the .fap is already close to.
 *
 * Drawn in the canvas's CURRENT color (same contract as ui_signal_bars), so it
 * survives an inverted/selected row. It neither sets nor restores a color.
 */
void ui_icon_radio(Canvas* canvas, int x, int y, bool ble);

/**
 * Draw `s` at (x, baseline), trimmed with a ".." marker if it would not fit
 * before `max_x`.
 *
 * Fixed-row canvas lists cannot wrap the way the Widget text elements they
 * replaced did, so without this a full-length SSID -- or a MAC on a row that
 * also carries an icon -- simply runs off the right edge and under whatever is
 * pinned there. Trimming visibly is the honest degradation; the untruncated
 * value is still on the detail screen and in the saved report.
 */
void ui_draw_str_fit(Canvas* canvas, int x, int baseline, const char* s, int max_x);
