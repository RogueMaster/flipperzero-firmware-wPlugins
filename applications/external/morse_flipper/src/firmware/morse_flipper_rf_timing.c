/*
 * Purpose: Store compact RF mark/space timing history.
 * Owns: RF timing ring buffer and diagnostic log formatting.
 * Depends on: morse_flipper_rf_timing.h.
 * Tests: tests/test_rf.c and tests/test_decoder_rf_integration.c.
 */

#include "morse_flipper_rf_timing.h"

#include <stdio.h>
#include <string.h>

static void rf_timing_push_log(MorseFlipperRfTiming* timing, const char* line) {
    size_t slot;

    if(!timing || !line) return;

    if(timing->log_count < sizeof(timing->log) / sizeof(timing->log[0])) {
        slot = (timing->log_start + timing->log_count) %
               (sizeof(timing->log) / sizeof(timing->log[0]));
        timing->log_count++;
    } else {
        slot = timing->log_start;
        timing->log_start =
            (timing->log_start + 1u) % (sizeof(timing->log) / sizeof(timing->log[0]));
    }

    snprintf(timing->log[slot], sizeof(timing->log[slot]), "%s", line);
}

void morse_flipper_rf_timing_init(MorseFlipperRfTiming* timing) {
    if(!timing) return;
    memset(timing, 0, sizeof(*timing));
}

void morse_flipper_rf_timing_capture(
    MorseFlipperRfTiming* timing,
    bool mark,
    uint16_t duration_ms,
    const char* frequency_text) {
    char line[48];
    size_t slot;

    if(!timing || !duration_ms) return;

    if(timing->count < sizeof(timing->durations_ms) / sizeof(timing->durations_ms[0])) {
        slot = (timing->start + timing->count) %
               (sizeof(timing->durations_ms) / sizeof(timing->durations_ms[0]));
        timing->count++;
    } else {
        slot = timing->start;
        timing->start = (timing->start + 1u) %
                        (sizeof(timing->durations_ms) / sizeof(timing->durations_ms[0]));
    }

    timing->marks[slot] = mark;
    timing->durations_ms[slot] = duration_ms;
    snprintf(
        line,
        sizeof(line),
        "rx %s %u @ %s",
        mark ? "mark" : "space",
        (unsigned)duration_ms,
        frequency_text ? frequency_text : "");
    rf_timing_push_log(timing, line);
}

size_t morse_flipper_rf_timing_count(const MorseFlipperRfTiming* timing) {
    return timing ? timing->count : 0;
}

bool morse_flipper_rf_timing_mark(const MorseFlipperRfTiming* timing, size_t idx) {
    if(!timing || idx >= timing->count) return false;
    return timing
        ->marks[(timing->start + idx) % (sizeof(timing->marks) / sizeof(timing->marks[0]))];
}

uint16_t morse_flipper_rf_timing_duration_ms(const MorseFlipperRfTiming* timing, size_t idx) {
    if(!timing || idx >= timing->count) return 0;
    return timing->durations_ms
        [(timing->start + idx) % (sizeof(timing->durations_ms) / sizeof(timing->durations_ms[0]))];
}

size_t morse_flipper_rf_timing_log_count(const MorseFlipperRfTiming* timing) {
    return timing ? timing->log_count : 0;
}

const char* morse_flipper_rf_timing_log_line(const MorseFlipperRfTiming* timing, size_t idx) {
    if(!timing || idx >= timing->log_count) return "";
    return timing->log[(timing->log_start + idx) % (sizeof(timing->log) / sizeof(timing->log[0]))];
}
