#include "mf_radio_draw.h"

#include <stdio.h>
#include <string.h>

#include <gui/canvas.h>

static void frequency_text(char* text, size_t size, uint32_t frequency_hz) {
    snprintf(text, size, "%06lu", (unsigned long)((frequency_hz / 1000U) % 1000000U));
}

static void draw_tx(const MfRadioState* state, Canvas* canvas) {
    char line[24];
    if(!state->snapshot.tx_allowed) {
        char digits[MF_RADIO_FREQ_DIGITS + 1U];
        frequency_text(digits, sizeof(digits), state->snapshot.frequency_hz);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, digits);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "TX Blocked");
        return;
    }
    snprintf(
        line,
        sizeof(line),
        "%lu khz",
        (unsigned long)(state->snapshot.frequency_hz / 1000U));
    state->draw_services->draw_tx_history(
        state->draw_services->context,
        canvas,
        &state->tx_history,
        state->decoder_services->preview(&state->decoder),
        state->decoder_services->preview_extendable(&state->decoder),
        line);
}

static void draw_frequency_digit(
    Canvas* canvas,
    int32_t x,
    bool focused,
    char digit) {
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
            canvas,
            7 + (19 * (int32_t)i),
            i == state->frequency_focus,
            digits[i]);
    if(!mf_radio_frequency_in_vfo(frequency_hz))
        status = "RX not available";
    else if(!state->hardware.frequency_valid(state->hardware.context, frequency_hz))
        status = "Invalid freq";
    else
        status = state->hardware.tx_allowed(state->hardware.context, frequency_hz) ?
                     "TX allowed" :
                     "RX only";
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, status);
}

static uint8_t rssi_width(int8_t dbm, uint8_t width) {
    if(dbm <= -115) return 0U;
    if(dbm >= -50) return width;
    return (uint8_t)(((int16_t)dbm + 115) * width / 65);
}

static void draw_rx_text(const MfRadioState* state, Canvas* canvas) {
    char text[sizeof(state->rx_text) + 2U];
    size_t len;
    uint8_t preview = state->decoder_services->preview(&state->decoder);
    snprintf(text, sizeof(text), "%s", state->rx_text);
    len = strlen(text);
    if(preview != 0U && preview != ' ' && preview != '|' && len + 1U < sizeof(text)) {
        text[len++] = (char)preview;
        text[len] = '\0';
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 10, text + (len > 54U ? len - 54U : 0U));
}

static void draw_ticker(const MfRadioState* state, Canvas* canvas, uint32_t now_ms) {
    uint8_t i;
    canvas_draw_line(canvas, 0, 34, 127, 34);
    for(i = 0U; i < state->ticker.count; i++) {
        const MfRadioTickerMark* mark =
            &state->ticker.marks[(state->ticker.start + i) % MF_RADIO_RX_TICKER_CAPACITY];
        uint32_t start_ms =
            mark->end_ms >= mark->duration_ms ? mark->end_ms - mark->duration_ms : 0U;
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
        canvas_draw_box(canvas, x0, mark->glitch ? 34 : 33, (size_t)(x1 - x0 + 1), mark->glitch ? 2U : 3U);
    }
}

static void draw_receive(const MfRadioState* state, Canvas* canvas, uint32_t now_ms) {
    char line[32];
    char wpm[16];
    uint8_t fill;
    uint8_t peak;
    uint8_t threshold;
    draw_rx_text(state, canvas);
    draw_ticker(state, canvas, now_ms);
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "%lu khz", (unsigned long)(state->snapshot.frequency_hz / 1000U));
    canvas_draw_str(canvas, 3, 44, line);
    snprintf(wpm, sizeof(wpm), "wpm %u", (unsigned)state->rx_wpm_hint);
    canvas_draw_str(canvas, 125 - (int32_t)canvas_string_width(canvas, wpm), 44, wpm);
    canvas_draw_frame(canvas, 3, 51, 122, 5);
    threshold = rssi_width(state->snapshot.monitor_threshold_dbm, 120U);
    canvas_draw_box(canvas, 2 + threshold, 47, 5, 1);
    if(state->rssi_valid) {
        fill = rssi_width(state->rssi_dbm, 120U);
        peak = rssi_width(state->rssi_peak_dbm, 120U);
        if(fill != 0U) canvas_draw_box(canvas, 4, 52, fill, 3);
        canvas_draw_box(canvas, 4 + peak, 52, 1, 3);
        snprintf(
            line,
            sizeof(line),
            "cs%u %d/%d",
            state->carrier_present ? 1U : 0U,
            state->rssi_dbm,
            state->rssi_peak_dbm);
    } else {
        snprintf(line, sizeof(line), "cs0 --/--");
    }
    canvas_draw_str(canvas, 3, 64, line);
    canvas_draw_str(canvas, 91, 64, "Bk exit");
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

