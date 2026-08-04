/*
 * Purpose: Maintain host-safe RF Morse transmit and receive state.
 * Owns: RF config, TX/RX logs, edge capture, and decoder feed helpers.
 * Depends on: morse_flipper_rf.h and optional app glue in FAP builds.
 * Tests: tests/test_rf.c and tests/test_decoder_rf_integration.c.
 */

#include "morse_flipper_rf.h"

#ifdef MORSE_FLIPPER_FAP
#include "morse_flipper_app_i.h"
#endif

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t min_khz;
    uint32_t max_khz;
} MorseFlipperRfBandKHz;

static const MorseFlipperRfBandKHz morse_flipper_rf_vfo_bands[] = {
    {MORSE_FLIPPER_RF_VFO_LOW_MIN_KHZ, MORSE_FLIPPER_RF_VFO_LOW_MAX_KHZ},
    {MORSE_FLIPPER_RF_VFO_MID_MIN_KHZ, MORSE_FLIPPER_RF_VFO_MID_MAX_KHZ},
    {MORSE_FLIPPER_RF_VFO_HIGH_MIN_KHZ, MORSE_FLIPPER_RF_VFO_HIGH_MAX_KHZ},
};

bool morse_flipper_rf_vfo_frequency_valid_khz(uint32_t khz) {
    size_t i;

    for(i = 0U; i < sizeof(morse_flipper_rf_vfo_bands) / sizeof(morse_flipper_rf_vfo_bands[0]);
        i++) {
        if(khz >= morse_flipper_rf_vfo_bands[i].min_khz &&
           khz <= morse_flipper_rf_vfo_bands[i].max_khz)
            return true;
    }

    return false;
}

bool morse_flipper_rf_vfo_frequency_valid_hz(uint32_t hz) {
    size_t i;

    for(i = 0U; i < sizeof(morse_flipper_rf_vfo_bands) / sizeof(morse_flipper_rf_vfo_bands[0]);
        i++) {
        if(hz >= morse_flipper_rf_vfo_bands[i].min_khz * 1000U &&
           hz <= morse_flipper_rf_vfo_bands[i].max_khz * 1000U)
            return true;
    }

    return false;
}

#ifdef MORSE_FLIPPER_FAP
static bool morse_flipper_rf_candidate_usable_hz(uint32_t hz) {
    return morse_flipper_rf_vfo_frequency_valid_hz(hz) && furi_hal_subghz_is_frequency_valid(hz) &&
           furi_hal_region_is_frequency_allowed(hz);
}

static bool morse_flipper_rf_region_wide_open(void) {
    const char* name = furi_hal_region_get_name();
    const FuriHalRegion* region = furi_hal_region_get();

    if(name != NULL && strcmp(name, "00") == 0) return true;
    if(region != NULL && region->bands_count == 1U && region->bands[0].start == 0U &&
       region->bands[0].end >= 1000000000U)
        return true;

    return furi_hal_region_is_frequency_allowed(MORSE_FLIPPER_RF_VFO_LOW_MIN_KHZ * 1000U) &&
           furi_hal_region_is_frequency_allowed(MORSE_FLIPPER_RF_VFO_MID_MIN_KHZ * 1000U) &&
           furi_hal_region_is_frequency_allowed(MORSE_FLIPPER_RF_VFO_HIGH_MIN_KHZ * 1000U);
}

static uint32_t morse_flipper_rf_band_default_hz(void) {
    const FuriHalRegion* region = furi_hal_region_get();
    size_t band_i;
    size_t vfo_i;

    if(region == NULL) return MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ;

    for(band_i = 0U; band_i < region->bands_count; band_i++) {
        uint32_t band_min_khz = (region->bands[band_i].start + 999U) / 1000U;
        uint32_t band_max_khz = region->bands[band_i].end / 1000U;

        for(vfo_i = 0U;
            vfo_i < sizeof(morse_flipper_rf_vfo_bands) / sizeof(morse_flipper_rf_vfo_bands[0]);
            vfo_i++) {
            uint32_t min_khz = band_min_khz > morse_flipper_rf_vfo_bands[vfo_i].min_khz ?
                                   band_min_khz :
                                   morse_flipper_rf_vfo_bands[vfo_i].min_khz;
            uint32_t max_khz = band_max_khz < morse_flipper_rf_vfo_bands[vfo_i].max_khz ?
                                   band_max_khz :
                                   morse_flipper_rf_vfo_bands[vfo_i].max_khz;
            uint32_t khz;

            if(min_khz > max_khz) continue;

            khz = min_khz + ((max_khz - min_khz) / 2U);
            if(morse_flipper_rf_candidate_usable_hz(khz * 1000U)) return khz * 1000U;
        }
    }

    return MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ;
}
#endif

uint32_t morse_flipper_rf_default_frequency_hz(void) {
#ifdef MORSE_FLIPPER_FAP
    static const uint32_t candidates_hz[] = {
        MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ,
        315000000U,
        868350000U,
        915000000U,
        920500000U,
    };
    size_t i;

    if(morse_flipper_rf_region_wide_open()) return MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ;

    for(i = 0U; i < sizeof(candidates_hz) / sizeof(candidates_hz[0]); i++) {
        if(morse_flipper_rf_candidate_usable_hz(candidates_hz[i])) return candidates_hz[i];
    }

    return morse_flipper_rf_band_default_hz();
#else
    return MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ;
#endif
}

static void rf_push_tx_log(MorseFlipperRf* rf, const char* line) {
    size_t slot;

    if(!rf || !line) return;

    if(rf->tx_log_count < sizeof(rf->tx_log) / sizeof(rf->tx_log[0])) {
        slot =
            (rf->tx_log_start + rf->tx_log_count) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
        rf->tx_log_count++;
    } else {
        slot = rf->tx_log_start;
        rf->tx_log_start = (rf->tx_log_start + 1u) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
    }

    snprintf(rf->tx_log[slot], sizeof(rf->tx_log[slot]), "%s", line);
}

static void rf_refresh_text(MorseFlipperRf* rf) {
    if(!rf) return;
    snprintf(
        rf->frequency_text,
        sizeof(rf->frequency_text),
        "%lu.%03lu",
        (unsigned long)(rf->frequency_hz / 1000000u),
        (unsigned long)((rf->frequency_hz / 1000u) % 1000u));
}

void morse_flipper_rf_init(MorseFlipperRf* rf) {
    if(!rf) return;
    memset(rf, 0, sizeof(*rf));
    morse_flipper_rf_timing_init(&rf->rx_timing);
    rf->frequency_hz = morse_flipper_rf_default_frequency_hz();
    rf_refresh_text(rf);
}

void morse_flipper_rf_set_frequency_hz(MorseFlipperRf* rf, uint32_t hz) {
    if(!rf) return;
    rf->frequency_hz = hz;
    rf_refresh_text(rf);
}

void morse_flipper_rf_reset_live(MorseFlipperRf* rf) {
    if(!rf) return;
    rf->tx_active = false;
    rf->tx_edges = 0;
    rf->tx_log_start = 0;
    rf->tx_log_count = 0;
    morse_flipper_rf_timing_init(&rf->rx_timing);
}

void morse_flipper_rf_handle_tx(MorseFlipperRf* rf, bool active, char symbol) {
    char line[48];

    if(!rf) return;
    if(rf->tx_active == active) return;

    rf->tx_active = active;
    rf->tx_edges++;
    snprintf(
        line,
        sizeof(line),
        "ook %s %c @ %s",
        active ? "on" : "off",
        symbol ? symbol : '?',
        rf->frequency_text);
    rf_push_tx_log(rf, line);
}

void morse_flipper_rf_capture_rx_timing(MorseFlipperRf* rf, bool mark, uint16_t duration_ms) {
    if(!rf || !duration_ms) return;
    morse_flipper_rf_timing_capture(&rf->rx_timing, mark, duration_ms, rf->frequency_text);
}

void morse_flipper_rf_ticker_reset(MorseFlipperRfTicker* ticker) {
    if(ticker == NULL) return;
    memset(ticker, 0, sizeof(*ticker));
}

void morse_flipper_rf_ticker_prune(MorseFlipperRfTicker* ticker, uint32_t now_ms) {
    if(ticker == NULL) return;

    while(ticker->count != 0U) {
        const MorseFlipperRfTickerMark* mark = &ticker->marks[ticker->start];

        if((uint32_t)(now_ms - mark->end_ms) <= MORSE_FLIPPER_RF_RX_TICKER_WINDOW_MS) {
            return;
        }

        ticker->start = (ticker->start + 1U) % MORSE_FLIPPER_RF_RX_TICKER_CAPACITY;
        ticker->count--;
    }
}

static void morse_flipper_rf_ticker_capture_inner(
    MorseFlipperRfTicker* ticker,
    bool mark,
    uint16_t duration_ms,
    uint32_t end_ms,
    bool glitch) {
    size_t slot;

    if(ticker == NULL || !mark || duration_ms == 0U) return;

    morse_flipper_rf_ticker_prune(ticker, end_ms);

    if(ticker->count < MORSE_FLIPPER_RF_RX_TICKER_CAPACITY) {
        slot = (ticker->start + ticker->count) % MORSE_FLIPPER_RF_RX_TICKER_CAPACITY;
        ticker->count++;
    } else {
        slot = ticker->start;
        ticker->start = (ticker->start + 1U) % MORSE_FLIPPER_RF_RX_TICKER_CAPACITY;
    }

    ticker->marks[slot] = (MorseFlipperRfTickerMark){
        .end_ms = end_ms,
        .duration_ms = duration_ms,
        .glitch = glitch,
    };
}

void morse_flipper_rf_ticker_capture(
    MorseFlipperRfTicker* ticker,
    bool mark,
    uint16_t duration_ms,
    uint32_t end_ms) {
    morse_flipper_rf_ticker_capture_inner(ticker, mark, duration_ms, end_ms, false);
}

void morse_flipper_rf_ticker_capture_glitch(
    MorseFlipperRfTicker* ticker,
    uint16_t duration_ms,
    uint32_t end_ms) {
    morse_flipper_rf_ticker_capture_inner(ticker, true, duration_ms, end_ms, true);
}

size_t morse_flipper_rf_ticker_count(const MorseFlipperRfTicker* ticker) {
    return ticker == NULL ? 0U : ticker->count;
}

bool morse_flipper_rf_ticker_mark(
    const MorseFlipperRfTicker* ticker,
    size_t idx,
    MorseFlipperRfTickerMark* out) {
    size_t slot;

    if(ticker == NULL || out == NULL || idx >= ticker->count) return false;

    slot = (ticker->start + idx) % MORSE_FLIPPER_RF_RX_TICKER_CAPACITY;
    *out = ticker->marks[slot];
    return true;
}

uint8_t morse_flipper_rf_clamp_wpm(uint8_t wpm) {
    if(wpm < MORSE_FLIPPER_RF_RX_WPM_MIN) return MORSE_FLIPPER_RF_RX_WPM_MIN;
    if(wpm > MORSE_FLIPPER_RF_RX_WPM_MAX) return MORSE_FLIPPER_RF_RX_WPM_MAX;
    return wpm;
}

uint8_t morse_flipper_rf_wpm_from_dit_ms(uint16_t dit_ms) {
    uint16_t wpm_tenths;

    if(dit_ms == 0U) return 0U;

    wpm_tenths = morse_flipper_rf_wpm_tenths_from_dit_ms(dit_ms);
    return (uint8_t)((wpm_tenths + 5U) / 10U);
}

uint16_t morse_flipper_rf_wpm_tenths_from_dit_ms(uint16_t dit_ms) {
    uint16_t wpm_tenths;
    const uint16_t min_tenths = (uint16_t)(MORSE_FLIPPER_RF_RX_WPM_MIN * 10U);
    const uint16_t max_tenths = (uint16_t)(MORSE_FLIPPER_RF_RX_WPM_MAX * 10U);

    if(dit_ms == 0U) return 0U;

    wpm_tenths = (uint16_t)((12000U + (dit_ms / 2U)) / dit_ms);
    if(wpm_tenths < min_tenths) return min_tenths;
    if(wpm_tenths > max_tenths) return max_tenths;
    return wpm_tenths;
}

uint16_t morse_flipper_rf_decoder_mark_ms(uint16_t duration_ms, uint16_t dit_ms) {
    uint32_t dah_threshold;

    if(duration_ms == 0U || dit_ms == 0U) return duration_ms;

    dah_threshold = ((uint32_t)dit_ms * 5U) / 2U;
    return (uint32_t)duration_ms < dah_threshold ? dit_ms : (uint16_t)(dit_ms * 3U);
}

const char* morse_flipper_rf_format_wpm_line(
    char* buf,
    size_t buf_sz,
    uint8_t hint_wpm,
    uint16_t tracked_dit_ms,
    bool tracked) {
    uint8_t wpm;
    uint16_t wpm_tenths;

    if(buf == NULL || buf_sz == 0U) return "";

    wpm = morse_flipper_rf_clamp_wpm(hint_wpm);
    if(tracked && tracked_dit_ms != 0U) {
        wpm_tenths = morse_flipper_rf_wpm_tenths_from_dit_ms(tracked_dit_ms);
        snprintf(
            buf,
            buf_sz,
            "auto wpm %u.%u",
            (unsigned)(wpm_tenths / 10U),
            (unsigned)(wpm_tenths % 10U));
        return buf;
    }

    snprintf(buf, buf_sz, "wpm %u", (unsigned)wpm);
    return buf;
}

uint32_t morse_flipper_rf_frequency_hz(const MorseFlipperRf* rf) {
    return rf ? rf->frequency_hz : 0;
}

uint32_t morse_flipper_rf_frequency_khz(const MorseFlipperRf* rf) {
    return rf ? (rf->frequency_hz / 1000u) : 0;
}

const char* morse_flipper_rf_frequency_text(const MorseFlipperRf* rf) {
    return rf ? rf->frequency_text : "";
}

size_t morse_flipper_rf_tx_log_count(const MorseFlipperRf* rf) {
    return rf ? rf->tx_log_count : 0;
}

const char* morse_flipper_rf_tx_log_line(const MorseFlipperRf* rf, size_t idx) {
    if(!rf || idx >= rf->tx_log_count) return "";
    return rf->tx_log[(rf->tx_log_start + idx) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]))];
}

size_t morse_flipper_rf_rx_count(const MorseFlipperRf* rf) {
    return rf ? morse_flipper_rf_timing_count(&rf->rx_timing) : 0;
}

bool morse_flipper_rf_rx_mark(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_mark(&rf->rx_timing, idx) : false;
}

uint16_t morse_flipper_rf_rx_duration_ms(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_duration_ms(&rf->rx_timing, idx) : 0;
}

size_t morse_flipper_rf_rx_log_count(const MorseFlipperRf* rf) {
    return rf ? morse_flipper_rf_timing_log_count(&rf->rx_timing) : 0;
}

const char* morse_flipper_rf_rx_log_line(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_log_line(&rf->rx_timing, idx) : "";
}

#ifdef MORSE_FLIPPER_FAP
int8_t morse_flipper_rssi_dbm_round(float rssi) {
    if(rssi >= 0.0f) return (int8_t)(rssi + 0.5f);
    return (int8_t)(rssi - 0.5f);
}

int8_t morse_flipper_rf_clamp_dbm(int8_t dbm) {
    if(dbm < -115) return -115;
    if(dbm > -50) return -50;
    return dbm;
}

bool morse_flipper_rf_frequency_valid_hz(uint32_t hz) {
    return morse_flipper_rf_vfo_frequency_valid_hz(hz) && furi_hal_subghz_is_frequency_valid(hz);
}

bool morse_flipper_rf_frequency_valid_khz(uint32_t khz) {
    return morse_flipper_rf_frequency_valid_hz(khz * 1000U);
}

static bool morse_flipper_rf_tx_allowed_hz(uint32_t hz) {
    return morse_flipper_rf_frequency_valid_hz(hz) && furi_hal_region_is_frequency_allowed(hz);
}

bool morse_flipper_rf_tx_allowed_khz(uint32_t khz) {
    return morse_flipper_rf_tx_allowed_hz(khz * 1000U);
}

void morse_flipper_rf_reset_rx_runtime(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->rf_live_active = true;
    app->rf_rssi_valid = false;
    app->rf_rssi_dbm = 0;
    app->rf_rssi_peak_dbm = 0;
    app->rf_rssi_sum_dbm = 0;
    app->rf_rssi_samples = 0U;
    app->rf_rssi_next_at = 0U;
    app->rf_rx_sample_next_at = 0U;
    app->rf_rx_view_next_at = 0U;
    app->rf_rx_edges_window = 0U;
    app->rf_rx_activity = 0U;
    app->rf_rssi_peak_decay_at = 0U;
    app->rf_carrier_present = false;
    app->rf_monitor_tone = false;
    app->rf_rx_level = false;
    app->rf_rx_candidate_level = false;
    app->rf_rx_candidate_samples = 0U;
    app->rf_rx_edge_at = 0U;
    app->rf_rx_gap_flushed = true;
    app->rf_rx_audio_enabled = true;
    app->rf_rx_wpm_hint = MORSE_FLIPPER_RF_RX_DEFAULT_WPM;
    app->rf_rx_text[0] = '\0';
    morse_flipper_rf_ticker_reset(&app->rf_rx_ticker);
    morse_flipper_rf_reset_live(&app->rf);
    morse_flipper_cw_decoder_init(
        &app->rf_decoder, morse_flipper_wpm_to_dit_ms(app->rf_rx_wpm_hint));
}

void morse_flipper_rf_rx_bump_wpm(MorseFlipperApp* app, int dir) {
    int next;

    if(app == NULL) return;

    next = app->rf_rx_wpm_hint == 0U ? MORSE_FLIPPER_RF_RX_DEFAULT_WPM : app->rf_rx_wpm_hint;
    next += dir;
    if(next < (int)MORSE_FLIPPER_RF_RX_WPM_MIN) next = MORSE_FLIPPER_RF_RX_WPM_MIN;
    if(next > (int)MORSE_FLIPPER_RF_RX_WPM_MAX) next = MORSE_FLIPPER_RF_RX_WPM_MAX;
    if(app->rf_rx_wpm_hint == (uint8_t)next) return;

    app->rf_rx_wpm_hint = (uint8_t)next;
    app->rf_monitor_tone = false;
    app->rf_rx_level = false;
    app->rf_rx_candidate_level = false;
    app->rf_rx_candidate_samples = 0U;
    app->rf_rx_edge_at = 0U;
    app->rf_rx_sample_next_at = 0U;
    app->rf_rx_gap_flushed = true;
    morse_flipper_cw_decoder_init(
        &app->rf_decoder, morse_flipper_wpm_to_dit_ms(app->rf_rx_wpm_hint));
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_rf_reset_edit(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->rf_edit_khz = morse_flipper_rf_frequency_khz(&app->rf);
    app->rf_freq_focus = 0U;
}

static uint32_t morse_flipper_rf_edit_place(uint8_t focus) {
    static const uint32_t places[MORSE_FLIPPER_RF_FREQ_DIGITS] = {
        100000U, 10000U, 1000U, 100U, 10U, 1U};

    if(focus >= MORSE_FLIPPER_RF_FREQ_DIGITS) return 1U;
    return places[focus];
}

void morse_flipper_rf_bump_focus(MorseFlipperApp* app, int dir) {
    uint8_t focus;

    if(app == NULL) return;
    focus = app->rf_freq_focus % MORSE_FLIPPER_RF_FREQ_DIGITS;
    if(dir < 0) {
        app->rf_freq_focus = focus == 0U ? (MORSE_FLIPPER_RF_FREQ_DIGITS - 1U) :
                                           (uint8_t)(focus - 1U);
    } else {
        app->rf_freq_focus = (uint8_t)((focus + 1U) % MORSE_FLIPPER_RF_FREQ_DIGITS);
    }
}

void morse_flipper_rf_bump_digit(MorseFlipperApp* app, int dir) {
    uint32_t place;
    uint32_t khz;
    uint8_t digit;
    uint8_t next_digit;
    int32_t delta;

    if(app == NULL) return;

    place = morse_flipper_rf_edit_place(app->rf_freq_focus);
    khz = app->rf_edit_khz % 1000000U;
    digit = (uint8_t)((khz / place) % 10U);
    next_digit = dir < 0 ? (uint8_t)((digit + 9U) % 10U) : (uint8_t)((digit + 1U) % 10U);
    delta = ((int32_t)next_digit - (int32_t)digit) * (int32_t)place;
    app->rf_edit_khz = (uint32_t)((int32_t)khz + delta);
}

void morse_flipper_rf_commit_edit(MorseFlipperApp* app) {
    uint32_t khz;

    if(app == NULL) return;

    khz = app->rf_edit_khz % 1000000U;
    if(!morse_flipper_rf_vfo_frequency_valid_khz(khz)) {
        app->rf_edit_khz = morse_flipper_rf_frequency_khz(&app->rf);
        return;
    }

    if(!morse_flipper_rf_frequency_valid_khz(khz))
        khz = morse_flipper_rf_default_frequency_hz() / 1000U;
    app->rf_edit_khz = khz;
    morse_flipper_rf_set_frequency_hz(&app->rf, khz * 1000U);
    morse_flipper_save_config(app);
}

const char* morse_flipper_rf_khz_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    unsigned long khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ;

    if(buf == NULL || buf_sz == 0U) return "433160 khz";
    if(app != NULL) khz = (unsigned long)morse_flipper_rf_frequency_khz(&app->rf);
    snprintf(buf, buf_sz, "%lu khz", khz);
    return buf;
}

const char* morse_flipper_rf_rssi_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(buf == NULL || buf_sz == 0U) return "";

    if(app == NULL || !app->rf_rssi_valid) {
        snprintf(buf, buf_sz, "cs0 --/--");
        return buf;
    }

    snprintf(
        buf,
        buf_sz,
        "cs%d %d/%d",
        app->rf_carrier_present ? 1 : 0,
        (int)app->rf_rssi_dbm,
        (int)app->rf_rssi_peak_dbm);
    return buf;
}

const char* morse_flipper_rf_rx_wpm_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    uint8_t hint_wpm = MORSE_FLIPPER_RF_RX_DEFAULT_WPM;
    uint16_t tracked_dit_ms = 0U;
    bool tracked = false;

    if(app != NULL) {
        hint_wpm = app->rf_rx_wpm_hint == 0U ? MORSE_FLIPPER_RF_RX_DEFAULT_WPM :
                                               app->rf_rx_wpm_hint;
        tracked_dit_ms = morse_flipper_cw_decoder_dit_ms(&app->rf_decoder);
        tracked = app->rf_decoder.dit_sample_count >= MORSE_FLIPPER_RF_RX_AUTO_WPM_SAMPLES;
    }

    return morse_flipper_rf_format_wpm_line(buf, buf_sz, hint_wpm, tracked_dit_ms, tracked);
}
#endif
