/*
 * Purpose: Publish compact RF timing buffers and helper API.
 * Owns: timing ring layout and diagnostic log limits.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_rf.c and tests/test_decoder_rf_integration.c.
 */

#ifndef yo3gnd_rf_timing_2230
#define yo3gnd_rf_timing_2230

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool marks[32];
    uint16_t durations_ms[32];
    size_t start;
    size_t count;
    char log[8][48];
    size_t log_start;
    size_t log_count;
} MorseFlipperRfTiming;

void morse_flipper_rf_timing_init(MorseFlipperRfTiming* timing);
void morse_flipper_rf_timing_capture(
    MorseFlipperRfTiming* timing,
    bool mark,
    uint16_t duration_ms,
    const char* frequency_text);
size_t morse_flipper_rf_timing_count(const MorseFlipperRfTiming* timing);
bool morse_flipper_rf_timing_mark(const MorseFlipperRfTiming* timing, size_t idx);
uint16_t morse_flipper_rf_timing_duration_ms(const MorseFlipperRfTiming* timing, size_t idx);
size_t morse_flipper_rf_timing_log_count(const MorseFlipperRfTiming* timing);
const char* morse_flipper_rf_timing_log_line(const MorseFlipperRfTiming* timing, size_t idx);

#endif
