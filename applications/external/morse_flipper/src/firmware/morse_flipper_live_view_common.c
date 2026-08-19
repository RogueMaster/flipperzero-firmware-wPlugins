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

uint16_t morse_flipper_star_anim_duration(uint8_t target_stars) {
    if(target_stars == 0U) return 0U;
    return (uint16_t)(((uint16_t)target_stars * MORSE_FLIPPER_STAR_FILL_MS) +
                      ((uint16_t)(target_stars - 1U) * MORSE_FLIPPER_STAR_GAP_MS));
}

void morse_flipper_start_star_animation(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    app->star_anim_started_at = now_ms == 0U ? 1U : now_ms;
    app->star_anim_next_redraw_ms = 0U;
}

uint8_t morse_flipper_star_anim_cols(
    uint32_t started_at,
    uint32_t now_ms,
    uint8_t star_idx,
    uint8_t target_stars) {
    uint32_t elapsed;
    uint32_t star_start;
    uint32_t star_elapsed;
    uint8_t cols;

    if(star_idx >= target_stars) return 0U;
    if(started_at == 0U) return MORSE_FLIPPER_STAR_REVEAL_COLS;

    elapsed = now_ms - started_at;
    star_start = (uint32_t)star_idx * (MORSE_FLIPPER_STAR_FILL_MS + MORSE_FLIPPER_STAR_GAP_MS);
    if(elapsed < star_start) return 0U;

    star_elapsed = elapsed - star_start;
    if(star_elapsed >= MORSE_FLIPPER_STAR_FILL_MS) return MORSE_FLIPPER_STAR_REVEAL_COLS;

    cols = (uint8_t)((star_elapsed * MORSE_FLIPPER_STAR_REVEAL_COLS) / MORSE_FLIPPER_STAR_FILL_MS);
    if(cols > MORSE_FLIPPER_STAR_REVEAL_COLS) cols = MORSE_FLIPPER_STAR_REVEAL_COLS;
    return cols;
}

static void
    morse_flipper_draw_star_glyph_fill_col(Canvas* canvas, uint8_t cx, uint8_t cy, int8_t rel_x) {
    if(rel_x < -2 || rel_x > 2) return;

    if(rel_x == -2 || rel_x == 2) {
        canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy - 1);
        canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy);
        canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy + 2);
        return;
    }

    canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy - 2);
    canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy - 1);
    canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy);
    canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy + 1);
    canvas_draw_dot(canvas, (int32_t)cx + rel_x, (int32_t)cy + 2);
}

void morse_flipper_draw_star_glyph_cols(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t cols) {
    static const int8_t points[10][2] = {
        {0, -4},
        {1, -1},
        {4, -1},
        {2, 1},
        {3, 4},
        {0, 2},
        {-3, 4},
        {-2, 1},
        {-4, -1},
        {-1, -1},
    };

    if(canvas == NULL) return;
    if(cols > MORSE_FLIPPER_STAR_REVEAL_COLS) cols = MORSE_FLIPPER_STAR_REVEAL_COLS;

    for(uint8_t i = 0U; i < 10U; i++) {
        uint8_t next = (uint8_t)((i + 1U) % 10U);
        canvas_draw_line(
            canvas,
            (int32_t)cx + points[i][0],
            (int32_t)cy + points[i][1],
            (int32_t)cx + points[next][0],
            (int32_t)cy + points[next][1]);
    }

    for(uint8_t i = 0U; i < cols; i++) {
        morse_flipper_draw_star_glyph_fill_col(canvas, cx, cy, (int8_t)i - 4);
    }
}

static void morse_flipper_draw_star_bitmap_pixel(
    Canvas* canvas,
    int32_t left,
    int32_t top,
    uint8_t row,
    uint8_t col,
    uint8_t scale) {
    canvas_draw_box(
        canvas, left + ((int32_t)col * scale), top + ((int32_t)row * scale), scale, scale);
}

static void morse_flipper_draw_star_bitmap_mask(
    Canvas* canvas,
    int32_t left,
    int32_t top,
    uint16_t mask,
    uint8_t row,
    uint8_t scale) {
    for(uint8_t col = 0U; col < MORSE_FLIPPER_STAR_REVEAL_COLS; col++) {
        if((mask & (uint16_t)(1U << (15U - col))) != 0U) {
            morse_flipper_draw_star_bitmap_pixel(canvas, left, top, row, col, scale);
        }
    }
}

void morse_flipper_draw_star_glyph_large_cols(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t cols) {
    enum {
        Scale = 1U,
        BitmapPx = 16U,
    };
    /* Source star is piskel2.png; face pixels are inverted over filled columns. */
    static const uint16_t source[BitmapPx] = {
        0x0000,
        0x0180,
        0x0240,
        0x0240,
        0x0420,
        0x7C3E,
        0x4002,
        0x2244,
        0x1008,
        0x0A50,
        0x1188,
        0x1008,
        0x23C4,
        0x4C32,
        0x700E,
        0x0000,
    };
    static const uint16_t body[BitmapPx] = {
        0x0000,
        0x0180,
        0x03C0,
        0x03C0,
        0x07E0,
        0x7FFE,
        0x7FFE,
        0x3FFC,
        0x1FF8,
        0x0FF0,
        0x1FF8,
        0x1FF8,
        0x3FFC,
        0x7C3E,
        0x700E,
        0x0000,
    };
    static const uint16_t face[BitmapPx] = {
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0240,
        0x0000,
        0x0240,
        0x0180,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    };
    int32_t left;
    int32_t top;

    if(canvas == NULL) return;
    if(cols > MORSE_FLIPPER_STAR_REVEAL_COLS) cols = MORSE_FLIPPER_STAR_REVEAL_COLS;

    left = (int32_t)cx - ((BitmapPx * Scale) / 2);
    top = (int32_t)cy - ((BitmapPx * Scale) / 2);

    for(uint8_t row = 0U; row < BitmapPx; row++) {
        morse_flipper_draw_star_bitmap_mask(canvas, left, top, source[row], row, Scale);
    }

    for(uint8_t col = 0U; col < cols; col++) {
        uint16_t col_mask = (uint16_t)(1U << (15U - col));

        for(uint8_t row = 0U; row < BitmapPx; row++) {
            if((body[row] & col_mask) != 0U) {
                morse_flipper_draw_star_bitmap_pixel(canvas, left, top, row, col, Scale);
            }
        }
    }

    canvas_set_color(canvas, ColorWhite);
    for(uint8_t col = 0U; col < cols; col++) {
        uint16_t col_mask = (uint16_t)(1U << (15U - col));

        for(uint8_t row = 0U; row < BitmapPx; row++) {
            if((face[row] & col_mask) != 0U) {
                morse_flipper_draw_star_bitmap_pixel(canvas, left, top, row, col, Scale);
            }
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

void morse_flipper_draw_star_glyph(Canvas* canvas, uint8_t cx, uint8_t cy, bool filled) {
    morse_flipper_draw_star_glyph_cols(
        canvas, cx, cy, filled ? MORSE_FLIPPER_STAR_REVEAL_COLS : 0U);
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
