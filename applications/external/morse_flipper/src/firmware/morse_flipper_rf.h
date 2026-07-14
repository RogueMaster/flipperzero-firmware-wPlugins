/*
 * Purpose: Define RF Morse state and host-safe RF operations.
 * Owns: frequency defaults, TX/RX log buffers, and RF timing aggregation.
 * Depends on: morse_flipper_rf_timing.h.
 * Tests: tests/test_rf.c and tests/test_decoder_rf_integration.c.
 */

#ifndef yo3gnd_rf_2239
#define yo3gnd_rf_2239

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "morse_flipper_rf_timing.h"

#define MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ  433160000u
#define MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ 433160u
#define MORSE_FLIPPER_RF_VFO_LOW_MIN_KHZ       300000u
#define MORSE_FLIPPER_RF_VFO_LOW_MAX_KHZ       348000u
#define MORSE_FLIPPER_RF_VFO_MID_MIN_KHZ       387000u
#define MORSE_FLIPPER_RF_VFO_MID_MAX_KHZ       464000u
#define MORSE_FLIPPER_RF_VFO_HIGH_MIN_KHZ      779000u
#define MORSE_FLIPPER_RF_VFO_HIGH_MAX_KHZ      928000u
#define MORSE_FLIPPER_RF_RX_DEFAULT_WPM        10u
#define MORSE_FLIPPER_RF_RX_WPM_MIN            2u
#define MORSE_FLIPPER_RF_RX_WPM_MAX            25u
#define MORSE_FLIPPER_RF_RX_SAMPLE_MS          8u
#define MORSE_FLIPPER_RF_RX_STABLE_SAMPLES     4u
#define MORSE_FLIPPER_RF_RX_VIEW_MS            32u
#define MORSE_FLIPPER_RF_RX_TICKER_WINDOW_MS   4000u
#define MORSE_FLIPPER_RF_RX_TICKER_CAPACITY    64u
#define MORSE_FLIPPER_RF_RX_HYSTERESIS_DB      4
#define MORSE_FLIPPER_RF_RX_AUTO_WPM_SAMPLES   3u

typedef struct {
    uint32_t end_ms;
    uint16_t duration_ms;
    bool glitch;
} MorseFlipperRfTickerMark;

typedef struct {
    MorseFlipperRfTickerMark marks[MORSE_FLIPPER_RF_RX_TICKER_CAPACITY];
    size_t start;
    size_t count;
} MorseFlipperRfTicker;

typedef struct {
    uint32_t frequency_hz;
    bool tx_active;
    uint32_t tx_edges;
    char tx_log[8][48];
    size_t tx_log_start;
    size_t tx_log_count;
    MorseFlipperRfTiming rx_timing;
    char frequency_text[16];
} MorseFlipperRf;

void morse_flipper_rf_init(MorseFlipperRf* rf);
bool morse_flipper_rf_vfo_frequency_valid_khz(uint32_t khz);
bool morse_flipper_rf_vfo_frequency_valid_hz(uint32_t hz);
void morse_flipper_rf_set_frequency_hz(MorseFlipperRf* rf, uint32_t hz);
void morse_flipper_rf_reset_live(MorseFlipperRf* rf);
void morse_flipper_rf_handle_tx(MorseFlipperRf* rf, bool active, char symbol);
void morse_flipper_rf_capture_rx_timing(MorseFlipperRf* rf, bool mark, uint16_t duration_ms);
void morse_flipper_rf_ticker_reset(MorseFlipperRfTicker* ticker);
void morse_flipper_rf_ticker_capture(
    MorseFlipperRfTicker* ticker,
    bool mark,
    uint16_t duration_ms,
    uint32_t end_ms);
void morse_flipper_rf_ticker_capture_glitch(
    MorseFlipperRfTicker* ticker,
    uint16_t duration_ms,
    uint32_t end_ms);
void morse_flipper_rf_ticker_prune(MorseFlipperRfTicker* ticker, uint32_t now_ms);
size_t morse_flipper_rf_ticker_count(const MorseFlipperRfTicker* ticker);
bool morse_flipper_rf_ticker_mark(
    const MorseFlipperRfTicker* ticker,
    size_t idx,
    MorseFlipperRfTickerMark* out);
uint8_t morse_flipper_rf_clamp_wpm(uint8_t wpm);
uint8_t morse_flipper_rf_wpm_from_dit_ms(uint16_t dit_ms);
uint16_t morse_flipper_rf_wpm_tenths_from_dit_ms(uint16_t dit_ms);
uint16_t morse_flipper_rf_decoder_mark_ms(uint16_t duration_ms, uint16_t dit_ms);
const char* morse_flipper_rf_format_wpm_line(
    char* buf,
    size_t buf_sz,
    uint8_t hint_wpm,
    uint16_t tracked_dit_ms,
    bool tracked);
uint32_t morse_flipper_rf_frequency_hz(const MorseFlipperRf* rf);
uint32_t morse_flipper_rf_frequency_khz(const MorseFlipperRf* rf);
const char* morse_flipper_rf_frequency_text(const MorseFlipperRf* rf);
size_t morse_flipper_rf_tx_log_count(const MorseFlipperRf* rf);
const char* morse_flipper_rf_tx_log_line(const MorseFlipperRf* rf, size_t idx);
size_t morse_flipper_rf_rx_count(const MorseFlipperRf* rf);
bool morse_flipper_rf_rx_mark(const MorseFlipperRf* rf, size_t idx);
uint16_t morse_flipper_rf_rx_duration_ms(const MorseFlipperRf* rf, size_t idx);
size_t morse_flipper_rf_rx_log_count(const MorseFlipperRf* rf);
const char* morse_flipper_rf_rx_log_line(const MorseFlipperRf* rf, size_t idx);

#endif
