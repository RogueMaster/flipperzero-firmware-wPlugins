/* Text widths without a canvas, for labels the stock modules would otherwise cut.
 * SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "wrap.h"

#include <stdint.h>

#define FONTW_MONO 6 /* FontKeyboard (profont11): every glyph advances 6 px */

/* WrapGlyphWidth callbacks: FontSecondary advance, FontKeyboard advance. */
uint8_t fontw_secondary_glyph(void* ctx, char c);
uint8_t fontw_mono_glyph(void* ctx, char c);

/* Width of `s` in FontSecondary as the firmware measures it (canvas_string_width). */
uint16_t fontw_secondary(const char* s);

/* Shortens `s` in place to at most `max_px` FontSecondary pixels: "..." at the end, never
 * after a space (stock elements_string_fit_width would leave "restart ..."). */
void fontw_fit_end(char* s, size_t size, uint16_t max_px);

/* Shortens `s` in place to its tail: "..." + as many trailing characters as fit. */
void fontw_fit_tail(char* s, size_t size, uint16_t max_px);
