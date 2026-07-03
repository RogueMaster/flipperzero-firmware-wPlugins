/*
 * Purpose: Lay out run-history text rows for the live view.
 * Owns: wrapping, underline placement, and token-aware glyph measurement.
 * Depends on: morse_flipper_run_layout.h and morse_flipper_cw_token.h.
 * Tests: tests/test_run_layout.c.
 */

#include "morse_flipper_run_layout.h"
#include "morse_flipper_cw_token.h"

#include <string.h>

static void morse_flipper_run_layout_push_row(
    MorseFlipperRunLayout* layout,
    const char* row,
    size_t row_len,
    uint8_t* row_n) {
    if(layout == NULL || row == NULL || row_n == NULL) return;

    if(*row_n < MORSE_FLIPPER_RUN_HISTORY_ROWS) {
        memcpy(layout->rows[*row_n], row, row_len + 1U);
        (*row_n)++;
        return;
    }

    memcpy(layout->rows[0], layout->rows[1], MORSE_FLIPPER_RUN_HISTORY_TEXT);
    memcpy(layout->rows[1], layout->rows[2], MORSE_FLIPPER_RUN_HISTORY_TEXT);
    memcpy(layout->rows[2], row, row_len + 1U);
}

static uint16_t morse_flipper_run_layout_ch_width(
    uint8_t ch,
    MorseFlipperGlyphWidthFn glyph_width,
    void* glyph_ctx) {
    const char* label;
    uint16_t w = 0U;

    if(!morse_flipper_cw_token_is_private(ch)) {
        w = glyph_width(ch, glyph_ctx);
        return w == 0U ? 1U : w;
    }

    if(ch == MORSE_FLIPPER_CW_TOKEN_AA) {
        w = glyph_width((uint8_t)'W', glyph_ctx);
        return w == 0U ? 8U : w;
    }

    label = morse_flipper_cw_token_label(ch);
    while(*label != '\0') {
        uint16_t part = glyph_width((uint8_t)*label++, glyph_ctx);

        w = (uint16_t)(w + (part == 0U ? 1U : part));
    }

    return w == 0U ? 1U : w;
}

void morse_flipper_run_layout_build(
    const char* text,
    bool underline_last_char,
    uint16_t max_w,
    MorseFlipperGlyphWidthFn glyph_width,
    void* glyph_ctx,
    MorseFlipperRunLayout* layout) {
    char row[MORSE_FLIPPER_RUN_HISTORY_TEXT];
    uint16_t row_w = 0U;
    uint16_t last_char_x = 0U;
    uint8_t last_char_w = 0U;
    uint8_t row_n = 0U;
    size_t row_len = 0U;
    uint16_t cursor_w;

    if(text == NULL || glyph_width == NULL || layout == NULL) return;

    cursor_w = glyph_width((uint8_t)'_', glyph_ctx);
    if(cursor_w == 0U) cursor_w = 1U;

    memset(layout, 0, sizeof(*layout));
    row[0] = '\0';

    while(*text) {
        uint8_t ch = (uint8_t)*text++;
        uint16_t ch_w;

        if(ch == '\n') {
            if(row_len != 0U) {
                morse_flipper_run_layout_push_row(layout, row, row_len, &row_n);
                row[0] = '\0';
                row_len = 0U;
                row_w = 0U;
            }
            continue;
        }

        if(ch == ' ' && row_len == 0U) continue;

        ch_w = morse_flipper_run_layout_ch_width(ch, glyph_width, glyph_ctx);

        if(row_len != 0U && row_w + ch_w > max_w) {
            morse_flipper_run_layout_push_row(layout, row, row_len, &row_n);
            row[0] = '\0';
            row_len = 0U;
            row_w = 0U;
            if(ch == ' ') continue;
        }

        if(row_len + 1U >= sizeof(row)) break;
        last_char_x = row_w;
        last_char_w = (uint8_t)ch_w;
        row[row_len++] = ch;
        row[row_len] = '\0';
        row_w += ch_w;

        if(ch == '=' || ch == MORSE_FLIPPER_CW_TOKEN_AA) {
            morse_flipper_run_layout_push_row(layout, row, row_len, &row_n);
            row[0] = '\0';
            row_len = 0U;
            row_w = 0U;
        }
    }

    if(underline_last_char && row_len != 0U) {
        layout->underline_valid = true;
        layout->underline_row =
            row_n < MORSE_FLIPPER_RUN_HISTORY_ROWS ? row_n : (MORSE_FLIPPER_RUN_HISTORY_ROWS - 1U);
        layout->underline_x = last_char_x;
        layout->underline_w = last_char_w == 0U ? 1U : last_char_w;
    } else {
        if(row_len != 0U && row_w + cursor_w > max_w) {
            morse_flipper_run_layout_push_row(layout, row, row_len, &row_n);
            row[0] = '\0';
            row_len = 0U;
            row_w = 0U;
        }

        layout->underline_valid = true;
        layout->underline_row =
            row_n < MORSE_FLIPPER_RUN_HISTORY_ROWS ? row_n : (MORSE_FLIPPER_RUN_HISTORY_ROWS - 1U);
        layout->underline_x = row_w;
        layout->underline_w = (uint8_t)cursor_w;
    }

    if(row_len != 0U || layout->underline_valid) {
        morse_flipper_run_layout_push_row(layout, row, row_len, &row_n);
    }
}
