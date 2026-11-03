/* Text layout shared by every view: word wrap and single-line fitting with "..." truncation.
 * Pure C (no Furi): glyph widths come from a callback, so the device measures with the
 * real font (canvas_glyph_width) and the host tests with a table.
 *
 * Rules:
 *   - break at spaces (the space is dropped) or after - / , : = (the character stays on
 *     the upper line); never inside a word unless the word alone is wider than the line;
 *   - a word split that way never leaves a 1-2 character fragment on the next line;
 *   - '\n' always starts a new line (blank lines are kept); '\r' is ignored;
 *   - leading spaces of a wrapped continuation line are skipped.
 *
 * Layout is resumable: wrapping again from any line's `off` reproduces the same lines, so
 * a view can keep a sparse index (every Nth line) instead of every line (RAM).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Advance width in pixels of one character in the current font. */
typedef uint8_t (*WrapGlyphWidth)(void* ctx, char c);

typedef struct {
    uint32_t off; /* byte offset into the text */
    uint16_t len; /* bytes on this line (no '\n') */
} WrapLine;

/* Lays `text[0..len)` out into lines at most `max_px` wide. Writes up to `max_lines` entries
 * to `out` (may be NULL) and returns the total number of lines the text needs. */
size_t wrap_text(
    const char* text,
    size_t len,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    WrapLine* out,
    size_t max_lines);

/* Like wrap_text, but stops after `max_lines` lines and stores in `*next` the offset where
 * the following line starts (`len` when the text is done). Returns the lines produced. */
size_t wrap_next(
    const char* text,
    size_t len,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    WrapLine* out,
    size_t max_lines,
    size_t* next);

/* Width in pixels of `s[0..len)`. */
uint32_t wrap_width(const char* s, size_t len, WrapGlyphWidth gw, void* ctx);

/* Copies `s` into `out` (size `out_size`), shortened with a trailing "..." if it is wider
 * than `max_px`. Returns true if it was shortened. */
bool wrap_fit(
    const char* s,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    char* out,
    size_t out_size);

/* One-line fit for file names: if `s` is wider than `max_px`, keep its start and its
 * extension and put "..." in the middle ("nas-20261021-061344.log" -> "nas-2026...4.log").
 * Returns true if it was shortened. */
bool wrap_fit_middle(
    const char* s,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    char* out,
    size_t out_size);
