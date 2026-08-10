/*
 * Purpose: Draw the once-per-day Listening streak intro.
 * Owns: compact streak intro layout and large digit placement.
 * Depends on: morse_flipper_app_i.h and the shared Terminus 24 prompt helper.
 * Tests: firmware build; UI rendering is hardware-only.
 */

#include "fonts/morse_flipper_terminus24.h"
#include "morse_flipper_app_i.h"

void morse_flipper_draw_streak_intro(Canvas* canvas, MorseFlipperApp* app) {
    char digits[6];
    size_t len;
    int32_t gap = 3;
    int32_t digit_w = (int32_t)MORSE_FLIPPER_TERMINUS24_WIDTH;
    int32_t run_w;
    int32_t start_x;
    size_t i;

    if(canvas == NULL || app == NULL) return;

    snprintf(digits, sizeof(digits), "%u", (unsigned)app->streak_intro_days);
    len = strlen(digits);
    if(len == 0U) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "current streak");

    run_w = ((int32_t)len * digit_w) + ((int32_t)(len - 1U) * gap);
    start_x = 64 - (run_w / 2) + (digit_w / 2);
    for(i = 0U; i < len; i++) {
        morse_flipper_draw_straight_prompt(
            canvas, start_x + ((digit_w + gap) * (int32_t)i), 34, (uint8_t)digits[i]);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "days");
}
