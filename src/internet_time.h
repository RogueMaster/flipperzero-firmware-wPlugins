#ifndef INTERNET_TIME_H
#define INTERNET_TIME_H

#include <stdbool.h>
#include <stdint.h>

/** Minimum supported UTC offset: UTC-12:00, in minutes. */
#define INTERNET_TIME_OFFSET_MIN_MINUTES (-12 * 60)

/** Maximum supported UTC offset: UTC+14:00, in minutes. */
#define INTERNET_TIME_OFFSET_MAX_MINUTES (14 * 60)

/** Offset step size in minutes (quarter-hour). */
#define INTERNET_TIME_OFFSET_STEP_MINUTES 15

/** Seconds in one civil day. */
#define INTERNET_TIME_DAY_SECONDS 86400

/** Beats in one civil day. */
#define INTERNET_TIME_BEATS_PER_DAY 1000

/**
 * Return true when utc_offset_minutes is in [UTC-12:00, UTC+14:00] and a
 * multiple of 15 minutes.
 */
bool internet_time_offset_valid(int16_t utc_offset_minutes);

/**
 * Convert BMT seconds-since-midnight to an integer beat in [0, 999].
 * Values outside [0, 86399] are normalized modulo 86400 first.
 */
uint16_t internet_time_beats_from_bmt_seconds(int32_t bmt_seconds);

/**
 * Convert local wall-clock fields plus a UTC offset into Swatch beats.
 * utc_offset_minutes is minutes east of UTC (e.g. +60 for UTC+01:00).
 * Local fields are treated as a time-of-day; day wrap is handled in both
 * directions when converting to fixed BMT (UTC+1, no DST).
 */
uint16_t internet_time_beats_from_local(
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    int16_t utc_offset_minutes);

#endif /* INTERNET_TIME_H */
