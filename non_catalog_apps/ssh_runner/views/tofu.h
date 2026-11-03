/* Text with TEXT_TOFU markers (core/textproc.h text_ascii): each marker is drawn as a hollow
 * box one glyph wide, the usual sign of a character the font lacks (a Chinese SSID shows as
 * boxes, not as garbage bytes). For the app's own views; the stock modules get '?' instead.
 * SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <gui/canvas.h>
#include <furi.h>

/* `font` must be the canvas's current font (its height sizes the box). */
uint16_t tofu_width(Canvas* canvas, Font font, const char* s);
/* Shortens `s` to `max_px` with a trailing "...", like elements_string_fit_width. */
void tofu_fit(Canvas* canvas, Font font, FuriString* s, uint16_t max_px);
/* As canvas_draw_str: `y` is the baseline. */
void tofu_draw_str(Canvas* canvas, Font font, int32_t x, int32_t y, const char* s);
