/*
 * Purpose: Publish run-history layout rows and underline metadata.
 * Owns: row buffers, glyph callback type, and layout result shape.
 * Depends on: morse_flipper_run_history.h.
 * Tests: tests/test_run_layout.c.
 */

#pragma once

#include "morse_flipper_run_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint16_t (*MorseFlipperGlyphWidthFn)(uint8_t ch, void* ctx);

typedef struct {
    char rows[MORSE_FLIPPER_RUN_HISTORY_ROWS][MORSE_FLIPPER_RUN_HISTORY_TEXT];
    uint8_t underline_row;
    uint16_t underline_x;
    uint8_t underline_w;
    bool underline_valid;
} MorseFlipperRunLayout;

void morse_flipper_run_layout_build(
    const char* text,
    bool underline_last_char,
    uint16_t max_w,
    MorseFlipperGlyphWidthFn glyph_width,
    void* glyph_ctx,
    MorseFlipperRunLayout* layout);
