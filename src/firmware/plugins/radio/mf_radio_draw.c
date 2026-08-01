#include "mf_radio_draw.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <gui/canvas.h>

static void frequency_text(char* text, size_t size, uint32_t frequency_hz) {
    snprintf(text, size, "%06lu", (unsigned long)((frequency_hz / 1000U) % 1000000U));
}

static void draw_tx(const MfRadioState* state, Canvas* canvas) {
    char line[24];
    const char* cwfm;
    int32_t cwfm_x;
    if(!state->snapshot.tx_allowed) {
        char digits[MF_RADIO_FREQ_DIGITS + 1U];
        frequency_text(digits, sizeof(digits), state->snapshot.frequency_hz);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, digits);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "TX Blocked");
        return;
    }
    snprintf(line, sizeof(line), "%lu khz", (unsigned long)(state->snapshot.frequency_hz / 1000U));
    state->draw_services->draw_tx_history(
        state->draw_services->context,
        canvas,
        &state->tx_history,
        state->decoder_services->preview(&state->decoder),
        state->decoder_services->preview_extendable(&state->decoder),
        line);
    cwfm = state->tx_mode == MfRadioTxModeCwfm ? "Right: CWFM on" : "Right: CWFM off";
    canvas_set_font(canvas, FontSecondary);
    cwfm_x = 125 - (int32_t)canvas_string_width(canvas, cwfm);
    if(state->tx_mode == MfRadioTxModeCwfm) cwfm_x--;
    canvas_draw_str(canvas, cwfm_x, 54, cwfm);
}

static void draw_frequency_digit(Canvas* canvas, int32_t x, bool focused, char digit) {
    char text[2] = {digit, '\0'};
    if(focused) {
        canvas_draw_triangle(canvas, x + 9, 9, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(canvas, x + 9, 33, 5, 3, CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, 11, 18, 20, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, 11, 18, 20, 1);
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, x + 9, 21, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_frequency(const MfRadioState* state, Canvas* canvas) {
    char digits[MF_RADIO_FREQ_DIGITS + 1U];
    const char* status;
    uint32_t frequency_hz = (state->edit_khz % 1000000U) * 1000U;
    uint8_t i;
    frequency_text(digits, sizeof(digits), frequency_hz);
    for(i = 0U; i < MF_RADIO_FREQ_DIGITS; i++)
        draw_frequency_digit(
            canvas, 7 + (19 * (int32_t)i), i == state->frequency_focus, digits[i]);
    canvas_set_font(canvas, FontSecondary);
    if(!mf_radio_frequency_in_vfo(frequency_hz)) {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, "RX not available");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "PLL lock failed");
        return;
    } else if(!state->hardware.frequency_valid(state->hardware.context, frequency_hz))
        status = "Invalid freq";
    else
        status = state->hardware.tx_allowed(state->hardware.context, frequency_hz) ? "TX allowed" :
                                                                                     "RX only";
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, status);
}

static uint8_t rssi_width(int8_t dbm, uint8_t width) {
    if(dbm <= -115) return 0U;
    if(dbm >= -50) return width;
    return (uint8_t)(((int16_t)dbm + 115) * width / 65);
}

static void format_wpm(const MfRadioState* state, char* text, size_t size) {
    uint16_t dit_ms = state->decoder_services->dit_ms(&state->decoder);
    if(state->decoder.dit_sample_count >= MF_RADIO_RX_AUTO_WPM_SAMPLES && dit_ms != 0U) {
        uint16_t wpm_tenths = (uint16_t)((12000U + (dit_ms / 2U)) / dit_ms);
        const uint16_t min_tenths = MF_RADIO_RX_WPM_MIN * 10U;
        const uint16_t max_tenths = MF_RADIO_RX_WPM_MAX * 10U;
        if(wpm_tenths < min_tenths) wpm_tenths = min_tenths;
        if(wpm_tenths > max_tenths) wpm_tenths = max_tenths;
        snprintf(
            text,
            size,
            "auto wpm %u.%u",
            (unsigned)(wpm_tenths / 10U),
            (unsigned)(wpm_tenths % 10U));
    } else {
        snprintf(text, size, "wpm %u", (unsigned)state->rx_wpm_hint);
    }
}

static void draw_rx_text(const MfRadioState* state, Canvas* canvas) {
    state->draw_services->draw_rx_text(
        state->draw_services->context,
        canvas,
        state->rx_text,
        state->decoder_services->preview(&state->decoder),
        state->decoder_services->preview_extendable(&state->decoder));
}

static void draw_ticker_mark(Canvas* canvas, const MfRadioTickerMark* mark, uint32_t now_ms) {
    uint32_t start_ms = mark->end_ms >= mark->duration_ms ? mark->end_ms - mark->duration_ms : 0U;
    uint32_t start_age = now_ms - start_ms;
    uint32_t end_age = now_ms - mark->end_ms;
    int32_t x0 = start_age >= MF_RADIO_RX_TICKER_WINDOW_MS ?
                     0 :
                     127 - (int32_t)(start_age * 127U / MF_RADIO_RX_TICKER_WINDOW_MS);
    int32_t x1 = end_age >= MF_RADIO_RX_TICKER_WINDOW_MS ?
                     0 :
                     127 - (int32_t)(end_age * 127U / MF_RADIO_RX_TICKER_WINDOW_MS);
    if(x1 < x0) {
        int32_t swap = x0;
        x0 = x1;
        x1 = swap;
    }
    canvas_draw_box(
        canvas, x0, mark->glitch ? 34 : 33, (size_t)(x1 - x0 + 1), mark->glitch ? 2U : 3U);
}

static void draw_ticker(const MfRadioState* state, Canvas* canvas, uint32_t now_ms) {
    uint8_t i;
    canvas_draw_line(canvas, 0, 34, 127, 34);
    for(i = 0U; i < state->ticker.count; i++) {
        const MfRadioTickerMark* mark =
            &state->ticker.marks[(state->ticker.start + i) % MF_RADIO_RX_TICKER_CAPACITY];
        draw_ticker_mark(canvas, mark, now_ms);
    }
    if(state->rx_level && state->rx_edge_at != 0U) {
        uint32_t duration_ms = now_ms - state->rx_edge_at;
        uint16_t dit_ms = state->decoder_services->dit_ms(&state->decoder);
        uint16_t glitch_limit_ms = dit_ms / 2U;
        uint16_t floor_ms = mf_radio_wpm_to_dit_ms(MF_RADIO_RX_WPM_MAX);
        MfRadioTickerMark mark;
        if(glitch_limit_ms < floor_ms) glitch_limit_ms = floor_ms;
        mark = (MfRadioTickerMark){
            .end_ms = now_ms,
            .duration_ms = duration_ms > UINT16_MAX ? UINT16_MAX : (uint16_t)duration_ms,
            .glitch = duration_ms < glitch_limit_ms,
        };
        draw_ticker_mark(canvas, &mark, now_ms);
    }
}

static void draw_receive(const MfRadioState* state, Canvas* canvas, uint32_t now_ms) {
    char line[32];
    char wpm[16];
    int32_t wpm_x;
    int32_t threshold_x;
    int32_t peak_x;
    uint8_t fill;
    uint8_t peak;
    uint8_t threshold;
    draw_rx_text(state, canvas);
    draw_ticker(state, canvas, now_ms);
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "%lu khz", (unsigned long)(state->snapshot.frequency_hz / 1000U));
    canvas_draw_str(canvas, 3, 44, line);
    format_wpm(state, wpm, sizeof(wpm));
    wpm_x = 125 - (int32_t)canvas_string_width(canvas, wpm);
    if(wpm_x < 66) wpm_x = 66;
    canvas_draw_str(canvas, wpm_x, 44, wpm);
    canvas_draw_frame(canvas, 3, 51, 122, 5);
    threshold = rssi_width(state->snapshot.monitor_threshold_dbm, 120U);
    threshold_x = 4 + threshold;
    if(threshold_x > 123) threshold_x = 123;
    canvas_draw_box(canvas, threshold_x - 2, 47, 5, 1);
    canvas_draw_box(canvas, threshold_x - 1, 48, 3, 1);
    canvas_draw_box(canvas, threshold_x, 49, 1, 1);
    if(state->rssi_valid) {
        fill = rssi_width(state->rssi_dbm, 120U);
        peak = rssi_width(state->rssi_peak_dbm, 120U);
        if(fill != 0U) canvas_draw_box(canvas, 4, 52, fill, 3);
        peak_x = 4 + peak;
        if(peak_x > 123) peak_x = 123;
        canvas_set_color(canvas, peak <= fill ? ColorWhite : ColorBlack);
        canvas_draw_box(canvas, peak_x, 52, 1, 3);
        canvas_set_color(canvas, ColorBlack);
        snprintf(
            line,
            sizeof(line),
            "cs%u r%d t%d",
            state->carrier_present ? 1U : 0U,
            state->rssi_dbm,
            state->snapshot.monitor_threshold_dbm);
    } else {
        snprintf(
            line,
            sizeof(line),
            "cs%u r-- t%d",
            state->carrier_present ? 1U : 0U,
            state->snapshot.monitor_threshold_dbm);
    }
    canvas_draw_str(canvas, 3, 64, line);
}

void mf_radio_draw(const MfRadioState* state, Canvas* canvas, uint32_t now_ms) {
    if(state == NULL || canvas == NULL || !state->entered || state->leaving) return;
    if(state->snapshot.page == MfRadioPageTransmit)
        draw_tx(state, canvas);
    else if(state->snapshot.page == MfRadioPageReceive)
        draw_receive(state, canvas, now_ms);
    else if(state->snapshot.page == MfRadioPageFrequency)
        draw_frequency(state, canvas);
}
