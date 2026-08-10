#ifndef CLOCK_MODEL_H
#define CLOCK_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t hour; /* 0-23 */
    uint8_t minute;
    uint8_t second;
    int16_t utc_offset_minutes;
    bool hour_format_24;
} ClockModelInput;

typedef struct {
    uint16_t beats; /* 0-999 */
    char beats_text[5]; /* "@ddd" + NUL */
    char local_time_text[16]; /* "HH:MM:SS" / "HH:MM:SS AM" + NUL */
} ClockModelSnapshot;

/**
 * Format beats as zero-padded "@ddd" into out (needs >= 5 bytes).
 * Returns false if out_size is too small.
 */
bool clock_model_format_beats(uint16_t beats, char* out, size_t out_size);

/**
 * Format local wall time into out. Uses 24-hour "HH:MM:SS" or
 * 12-hour "HH:MM:SS AM"/"HH:MM:SS PM" (midnight=12 AM, noon=12 PM).
 * Returns false if out_size is too small.
 */
bool clock_model_format_local_time(
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    bool hour_format_24,
    char* out,
    size_t out_size);

/**
 * Build a display snapshot from RTC-like local fields and UTC offset.
 * Invalid offsets still produce a snapshot using the given offset for beat
 * math; callers should validate offsets separately for settings UX.
 */
void clock_model_build_snapshot(const ClockModelInput* input, ClockModelSnapshot* out);

#endif /* CLOCK_MODEL_H */
