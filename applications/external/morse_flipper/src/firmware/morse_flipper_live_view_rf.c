/*
 * Purpose: Draw RF TX/RX, frequency edit, and trace screens.
 * Owns: RF live-view layout, frequency digits, and trace display text.
 * Depends on: morse_flipper_app_i.h and RF runtime state.
 * Tests: tests/test_rf.c covers data paths; rendering is hardware-only.
 */

#include "morse_flipper_app_i.h"
#include "morse_flipper_run_layout.h"

#include <string.h>

static void morse_flipper_rf_edit_text(char* out, size_t out_sz, uint32_t khz) {
    if(out == NULL || out_sz == 0U) return;
    snprintf(out, out_sz, "%06lu", (unsigned long)(khz % 1000000U));
}

void morse_flipper_draw_rf_tx_blocked(Canvas* canvas, const MorseFlipperApp* app) {
    char freq[MORSE_FLIPPER_RF_FREQ_DIGITS + 1U];

    if(canvas == NULL || app == NULL) return;

    morse_flipper_rf_edit_text(freq, sizeof(freq), morse_flipper_rf_frequency_khz(&app->rf));
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, freq);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "TX Blocked");
}

static void morse_flipper_draw_rf_freq_digit(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t w,
    size_t h,
    bool focused,
    char digit) {
    char text[2] = {digit, '\0'};

    if(canvas == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_triangle(
            canvas, x + (int32_t)(w / 2U), y - 2, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas, x + (int32_t)(w / 2U), y + (int32_t)h + 2, 5, 3, CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, y, w, h, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 1);
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, x + (int32_t)(w / 2U), y + (int32_t)(h / 2U), AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

void morse_flipper_draw_rf_freq_picker(Canvas* canvas, const MorseFlipperApp* app) {
    char digits[MORSE_FLIPPER_RF_FREQ_DIGITS + 1U];
    const char* tx_line;
    uint32_t edit_khz;
    const int32_t cell_w = 18;
    const int32_t cell_h = 20;
    const int32_t gap = 1;
    const int32_t total_w = (cell_w * (int32_t)MORSE_FLIPPER_RF_FREQ_DIGITS) +
                            (gap * ((int32_t)MORSE_FLIPPER_RF_FREQ_DIGITS - 1));
    const int32_t x0 = (128 - total_w) / 2;
    const int32_t y0 = 11;
    uint8_t i;

    if(canvas == NULL || app == NULL) return;

    edit_khz = app->rf_edit_khz % 1000000U;
    morse_flipper_rf_edit_text(digits, sizeof(digits), edit_khz);
    for(i = 0U; i < MORSE_FLIPPER_RF_FREQ_DIGITS; i++) {
        morse_flipper_draw_rf_freq_digit(
            canvas,
            x0 + ((cell_w + gap) * (int32_t)i),
            y0,
            (size_t)cell_w,
            (size_t)cell_h,
            i == (app->rf_freq_focus % MORSE_FLIPPER_RF_FREQ_DIGITS),
            digits[i]);
    }

    canvas_set_font(canvas, FontSecondary);
    if(!morse_flipper_rf_vfo_frequency_valid_khz(edit_khz)) {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, "RX not available");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "PLL lock failed");
        return;
    }

    if(!morse_flipper_rf_frequency_valid_khz(edit_khz)) {
        tx_line = "Invalid freq";
    } else {
        tx_line = morse_flipper_rf_tx_allowed_khz(edit_khz) ? "TX allowed" : "RX only";
    }

    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, tx_line);
}

static uint8_t morse_flipper_rf_rssi_bar_px(int8_t dbm, uint8_t width) {
    const int16_t lo = -115;
    const int16_t hi = -50;
    int16_t clamped = dbm;

    if(clamped <= lo) return 0U;
    if(clamped >= hi) return width;
    return (uint8_t)(((clamped - lo) * width) / (hi - lo));
}

static void morse_flipper_draw_rf_rssi_bar(Canvas* canvas, const MorseFlipperApp* app) {
    const int32_t x = 3;
    const int32_t y = 51;
    const uint8_t w = 122;
    const uint8_t h = 5;
    const uint8_t inner_w = w - 2U;
    uint8_t fill_w = 0U;
    uint8_t peak_w = 0U;
    uint8_t thr_w = 0U;
    int32_t peak_x;
    int32_t thr_x;

    if(canvas == NULL || app == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);

    thr_w = morse_flipper_rf_rssi_bar_px(app->rf_monitor_threshold_dbm, inner_w);
    thr_x = x + 1 + thr_w;
    if(thr_x > x + (int32_t)inner_w) thr_x = x + (int32_t)inner_w;
    canvas_draw_box(canvas, thr_x - 2, y - 4, 5, 1);
    canvas_draw_box(canvas, thr_x - 1, y - 3, 3, 1);
    canvas_draw_box(canvas, thr_x, y - 2, 1, 1);

    if(!app->rf_rssi_valid) return;

    fill_w = morse_flipper_rf_rssi_bar_px(app->rf_rssi_dbm, inner_w);
    peak_w = morse_flipper_rf_rssi_bar_px(app->rf_rssi_peak_dbm, inner_w);

    if(fill_w != 0U) canvas_draw_box(canvas, x + 1, y + 1, fill_w, h - 2U);

    peak_x = x + 1 + peak_w;
    if(peak_x > x + (int32_t)inner_w) peak_x = x + (int32_t)inner_w;
    canvas_set_color(canvas, peak_w <= fill_w ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, peak_x, y + 1, 1, h - 2U);
    canvas_set_color(canvas, ColorBlack);
}

static uint16_t morse_flipper_rf_canvas_glyph_width(uint8_t ch, void* ctx) {
    Canvas* canvas = ctx;
    if(canvas == NULL) return 0U;
    return (uint16_t)canvas_glyph_width(canvas, ch);
}

static int32_t morse_flipper_rf_ticker_x(uint32_t event_ms, uint32_t now_ms) {
    uint32_t age_ms = now_ms - event_ms;

    if(age_ms >= MORSE_FLIPPER_RF_RX_TICKER_WINDOW_MS) return 0;
    return 127 - (int32_t)((age_ms * 127U) / MORSE_FLIPPER_RF_RX_TICKER_WINDOW_MS);
}

static uint16_t morse_flipper_rf_ticker_dit_ms(const MorseFlipperApp* app) {
    uint16_t dit_ms;
    uint8_t hint_wpm;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;

    dit_ms = morse_flipper_cw_decoder_dit_ms(&app->rf_decoder);
    if(dit_ms != 0U) return dit_ms;

    hint_wpm = app->rf_rx_wpm_hint == 0U ? MORSE_FLIPPER_RF_RX_DEFAULT_WPM : app->rf_rx_wpm_hint;
    return morse_flipper_wpm_to_dit_ms(hint_wpm);
}

static uint16_t morse_flipper_rf_ticker_glitch_limit_ms(uint16_t dit_ms) {
    uint16_t limit_ms;
    uint16_t floor_ms;

    floor_ms = morse_flipper_wpm_to_dit_ms(MORSE_FLIPPER_RF_RX_WPM_MAX);
    limit_ms = dit_ms / 2U;
    return limit_ms < floor_ms ? floor_ms : limit_ms;
}

static void morse_flipper_draw_rf_rx_ticker_mark(
    Canvas* canvas,
    const MorseFlipperRfTickerMark* mark,
    uint16_t dit_ms,
    uint32_t now_ms) {
    uint32_t start_ms;
    int32_t x0;
    int32_t x1;
    int32_t w;

    if(canvas == NULL || mark == NULL) return;

    start_ms = mark->end_ms >= mark->duration_ms ? mark->end_ms - mark->duration_ms : 0U;
    x0 = morse_flipper_rf_ticker_x(start_ms, now_ms);
    x1 = morse_flipper_rf_ticker_x(mark->end_ms, now_ms);
    if(x1 < x0) {
        int32_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if(x1 < 0 || x0 > 127) return;
    if(x0 < 0) x0 = 0;
    if(x1 > 127) x1 = 127;

    if(mark->glitch) {
        w = x1 - x0 + 1;
        if(w < 1) w = 1;
        canvas_draw_box(canvas, x0, 34, (size_t)w, 2);
        return;
    }

    UNUSED(dit_ms);
    w = x1 - x0 + 1;
    if(w < 1) w = 1;
    canvas_draw_box(canvas, x0, 33, (size_t)w, 3);
}

static void
    morse_flipper_draw_rf_rx_ticker(Canvas* canvas, const MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperRfTickerMark mark;
    size_t count;
    uint16_t dit_ms;

    if(canvas == NULL || app == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 34, 127, 34);

    dit_ms = morse_flipper_rf_ticker_dit_ms(app);
    count = morse_flipper_rf_ticker_count(&app->rf_rx_ticker);
    for(size_t i = 0U; i < count; i++) {
        if(!morse_flipper_rf_ticker_mark(&app->rf_rx_ticker, i, &mark)) continue;
        morse_flipper_draw_rf_rx_ticker_mark(canvas, &mark, dit_ms, now_ms);
    }

    if(app->rf_rx_level && app->rf_rx_edge_at != 0U) {
        uint32_t duration_ms = now_ms - app->rf_rx_edge_at;
        if(duration_ms != 0U) {
            uint16_t glitch_limit_ms = morse_flipper_rf_ticker_glitch_limit_ms(dit_ms);
            mark = (MorseFlipperRfTickerMark){
                .end_ms = now_ms,
                .duration_ms = duration_ms > 65535U ? 65535U : (uint16_t)duration_ms,
                .glitch = duration_ms < glitch_limit_ms,
            };
            morse_flipper_draw_rf_rx_ticker_mark(canvas, &mark, dit_ms, now_ms);
        }
    }
}

static void morse_flipper_draw_rf_rx_text(Canvas* canvas, const MorseFlipperApp* app) {
    MorseFlipperRunLayout layout;
    char text[sizeof(app->rf_rx_text) + 2U];
    char preview;
    bool preview_active;
    bool preview_extendable;
    static const uint8_t row_y[MORSE_FLIPPER_RUN_HISTORY_ROWS] = {9U, 19U, 29U};

    if(canvas == NULL || app == NULL) return;

    snprintf(text, sizeof(text), "%s", app->rf_rx_text);
    preview = morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->rf_decoder));
    preview_active = preview != 0 && preview != ' ' && preview != '|';
    preview_extendable = morse_flipper_cw_decoder_preview_extendable(&app->rf_decoder);
    if(preview_active) {
        size_t len = strlen(text);

        if(len + 1U < sizeof(text)) {
            text[len] = preview;
            text[len + 1U] = '\0';
        }
    }

    canvas_set_font(canvas, FontSecondary);
    morse_flipper_run_layout_build(
        text,
        preview_active && preview_extendable,
        126U,
        morse_flipper_rf_canvas_glyph_width,
        canvas,
        &layout);
    for(size_t i = 0U; i < MORSE_FLIPPER_RUN_HISTORY_ROWS; i++) {
        morse_flipper_draw_run_text(canvas, 1, row_y[i], layout.rows[i]);
    }
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
}

void morse_flipper_draw_rf_rx_screen(Canvas* canvas, MorseFlipperApp* app) {
    char freq_line[32];
    char wpm_line[16];
    char rssi_line[32];
    uint32_t now_ms;
    int32_t wpm_x;

    if(canvas == NULL || app == NULL) return;

    now_ms = furi_get_tick();
    morse_flipper_draw_rf_rx_text(canvas, app);
    morse_flipper_draw_rf_rx_ticker(canvas, app, now_ms);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 44, morse_flipper_rf_khz_line(app, freq_line, sizeof(freq_line)));
    morse_flipper_rf_rx_wpm_line(app, wpm_line, sizeof(wpm_line));
    wpm_x = 125 - (int32_t)canvas_string_width(canvas, wpm_line);
    if(wpm_x < 66) wpm_x = 66;
    canvas_draw_str(canvas, wpm_x, 44, wpm_line);
    morse_flipper_draw_rf_rssi_bar(canvas, app);
    canvas_draw_str(canvas, 3, 64, morse_flipper_rf_rssi_line(app, rssi_line, sizeof(rssi_line)));
    canvas_draw_str(canvas, 125 - canvas_string_width(canvas, "Bk exit"), 64, "Bk exit");
}
