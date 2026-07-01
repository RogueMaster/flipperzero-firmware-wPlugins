/*
 * Purpose: Provide shared drawing helpers for live-view screens.
 * Owns: common hints, dividers, progress, and prompt glyph rendering.
 * Depends on: morse_flipper_app_i.h and fonts/morse_flipper_terminus24.h.
 * Tests: tests/test_prompt_font.c plus firmware build.
 */

#include "fonts/morse_flipper_terminus24.h"
#include "morse_flipper_app_i.h"

void morse_flipper_draw_left_exit_hint(Canvas* canvas) {
    canvas_draw_box(canvas, 124, 32, 1, 1);
    canvas_draw_box(canvas, 125, 31, 1, 3);
    canvas_draw_box(canvas, 126, 30, 1, 5);
    canvas_draw_box(canvas, 127, 29, 1, 7);
}

void morse_flipper_draw_tx_history_divider(Canvas* canvas, bool left_hint) {
    if(canvas == NULL) return;

    if(!left_hint) {
        canvas_draw_line(canvas, 0, 34, 127, 34);
        return;
    }

    canvas_draw_line(canvas, 0, 34, 119, 34);
    canvas_draw_box(canvas, 124, 34, 1, 1);
    canvas_draw_box(canvas, 125, 33, 1, 3);
    canvas_draw_box(canvas, 126, 32, 1, 5);
    canvas_draw_box(canvas, 127, 31, 1, 7);
}

uint8_t morse_flipper_live_upper_char(uint8_t ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

void morse_flipper_draw_straight_prompt(Canvas* canvas, int32_t cx, int32_t cy, uint8_t ch) {
    const MorseFlipperTerminus24Glyph* glyph;
    int32_t x0;
    int32_t y0;
    size_t row;
    size_t x_max;
    size_t y_max;

    if(canvas == NULL) return;

    glyph = morse_flipper_terminus24_glyph(morse_flipper_live_upper_char(ch));
    x0 = cx - (int32_t)(MORSE_FLIPPER_TERMINUS24_WIDTH / 2U);
    y0 = cy - (int32_t)(MORSE_FLIPPER_TERMINUS24_HEIGHT / 2U);
    x_max = canvas_width(canvas);
    y_max = canvas_height(canvas);

    for(row = 0U; row < MORSE_FLIPPER_TERMINUS24_HEIGHT; row++) {
        uint16_t bits = morse_flipper_terminus24_row(glyph, row);
        uint8_t col;

        for(col = 0U; col < MORSE_FLIPPER_TERMINUS24_WIDTH; col++) {
            if((bits & (uint16_t)(1U << (11U - col))) != 0U) {
                int32_t px = x0 + (int32_t)col;
                int32_t py = y0 + (int32_t)row;

                if(px >= 0 && py >= 0 && (size_t)px < x_max && (size_t)py < y_max) {
                    canvas_draw_dot(canvas, px, py);
                }
            }
        }
    }
}

static void morse_flipper_gpio_probe_pair_text(
    const MorseFlipperApp* app,
    uint8_t pin_idx,
    char* out,
    size_t out_sz) {
    if(app == NULL || out == NULL || out_sz == 0U) return;
    snprintf(
        out,
        out_sz,
        "%s - %s",
        morse_flipper_gpio_name(app->gpio_ground_idx),
        morse_flipper_gpio_name(pin_idx));
}

void morse_flipper_draw_gpio_probe_overlay(Canvas* canvas, const MorseFlipperApp* app) {
    char pair_a[24];
    char pair_b[32];

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    if(morse_flipper_gpio_probe_blocks_start(app)) {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Short circuit");
        canvas_set_font(canvas, FontSecondary);
        if(app->gpio_probe_state == MorseFlipperGpioProbeGroundToBoth) {
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_b, sizeof(pair_b));
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, pair_a);
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, pair_b);
        } else {
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_a, sizeof(pair_a));
            canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 58, AlignCenter, AlignCenter, "Do not press the paddles");
        return;
    }

    morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
    snprintf(pair_b, sizeof(pair_b), "%s shorted", pair_a);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, pair_b);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Mono jack in stereo plug?");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Switching to SK mode");
}

void morse_flipper_draw_startup_gpio_probe(Canvas* canvas, const MorseFlipperApp* app) {
    char pair_a[24];
    char pair_b[24];

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Short circuit");
    canvas_set_font(canvas, FontSecondary);

    if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToBoth) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_b, sizeof(pair_b));
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, pair_a);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, pair_b);
    } else if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    } else if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToDit) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    }

    if(!morse_flipper_gpio_probe_forces_straight(app->startup_gpio_probe_state)) {
        canvas_draw_str_aligned(
            canvas, 64, 58, AlignCenter, AlignCenter, "Press back to continue");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Mono jack in stereo plug?");
    canvas_draw_str_aligned(
        canvas, 64, 58, AlignCenter, AlignCenter, "Press OK to switch to SK mode");
}
