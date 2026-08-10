/*
 * Purpose: Draw Free Practice run and TX history screens.
 * Owns: run text layout, prompt glyph drawing, and history presentation.
 * Depends on: morse_flipper_app_i.h and morse_flipper_run_layout.h.
 * Tests: tests/test_run_layout.c and tests/test_run_history.c cover layout data.
 */

#include "morse_flipper_app_i.h"
#include "morse_flipper_cw_token.h"
#include "morse_flipper_run_layout.h"

static uint16_t morse_flipper_canvas_glyph_width(uint8_t ch, void* ctx) {
    Canvas* canvas = ctx;
    if(canvas == NULL) return 0U;
    return (uint16_t)canvas_glyph_width(canvas, ch);
}

void morse_flipper_draw_run_text(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    static const uint8_t aa_glyph[] = {
        0x02U,
        0x02U,
        0x12U,
        0x32U,
        0x7EU,
        0x30U,
        0x10U,
    };

    if(canvas == NULL || text == NULL) return;

    while(*text != '\0') {
        uint8_t ch = (uint8_t)*text++;

        if(ch == MORSE_FLIPPER_CW_TOKEN_AA) {
            for(size_t row = 0U; row < sizeof(aa_glyph) / sizeof(aa_glyph[0]); row++) {
                for(size_t col = 0U; col < 8U; col++) {
                    if((aa_glyph[row] & (uint8_t)(1U << (7U - col))) != 0U) {
                        canvas_draw_dot(canvas, x + (int32_t)col, y - 7 + (int32_t)row);
                    }
                }
            }
            x += 8;
            continue;
        }

        if(morse_flipper_cw_token_is_private(ch)) {
            const char* label = morse_flipper_cw_token_label(ch);
            uint16_t w = (uint16_t)canvas_string_width(canvas, label);

            canvas_draw_str(canvas, x, y, label);
            if(w == 0U) w = 1U;
            canvas_draw_line(canvas, x, y - 8, x + (int32_t)w - 1, y - 8);
            x += w;
            continue;
        }

        char s[2] = {(char)ch, '\0'};
        canvas_draw_str(canvas, x, y, s);
        x += (int32_t)canvas_glyph_width(canvas, ch);
    }
}

void morse_flipper_draw_tx_history_screen_custom(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line,
    const char* hint_override) {
    MorseFlipperRunHistory preview_history;
    MorseFlipperRunLayout layout;
    char mode_line[32];
    char hint_line[32];
    const char* footer;
    char preview;
    bool preview_extendable;
    bool left_hint;
    static const uint8_t row_y[MORSE_FLIPPER_RUN_HISTORY_ROWS] = {10U, 20U, 30U};

    if(canvas == NULL || app == NULL) return;

    preview_history = app->run_history;
    preview = morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    preview_extendable = morse_flipper_cw_decoder_preview_extendable(&app->tx_decoder);
    if(preview != 0 && preview != ' ' && preview != '|') {
        char preview_text[2] = {preview, '\0'};
        morse_flipper_run_history_append(&preview_history, preview_text);
    }

    left_hint = morse_flipper_live_left_hint(app);

    canvas_set_font(canvas, FontSecondary);
    morse_flipper_run_layout_build(
        morse_flipper_run_history_text(&preview_history),
        preview != 0 && preview != ' ' && preview != '|' && preview_extendable,
        126U,
        morse_flipper_canvas_glyph_width,
        canvas,
        &layout);
    morse_flipper_draw_run_text(canvas, 1, row_y[0], layout.rows[0]);
    morse_flipper_draw_run_text(canvas, 1, row_y[1], layout.rows[1]);
    morse_flipper_draw_run_text(canvas, 1, row_y[2], layout.rows[2]);
    if(layout.underline_valid && layout.underline_row < MORSE_FLIPPER_RUN_HISTORY_ROWS) {
        uint16_t underline_x = (uint16_t)(1U + layout.underline_x);
        uint8_t underline_w = layout.underline_w == 0U ? 1U : layout.underline_w;
        uint8_t underline_y = (uint8_t)(row_y[layout.underline_row] + 2U);
        canvas_draw_line(
            canvas,
            (int32_t)underline_x,
            underline_y,
            (int32_t)(underline_x + underline_w - 1U),
            underline_y);
    }
    morse_flipper_draw_tx_history_divider(canvas, left_hint);
    canvas_draw_str(canvas, 3, 44, morse_flipper_run_mode_line(app, mode_line, sizeof(mode_line)));
    canvas_draw_str(canvas, 3, 54, second_line ? second_line : "");
    footer = hint_override ? hint_override :
                             morse_flipper_run_hint(app, hint_line, sizeof(hint_line));
    if(canvas_string_width(canvas, footer) > 124) canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 3, 64, footer);
}

void morse_flipper_draw_tx_history_screen(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line) {
    morse_flipper_draw_tx_history_screen_custom(canvas, app, second_line, NULL);
}
