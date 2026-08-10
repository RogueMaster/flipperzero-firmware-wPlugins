#include "internet_time.h"

bool internet_time_offset_valid(int16_t utc_offset_minutes) {
    if(utc_offset_minutes < INTERNET_TIME_OFFSET_MIN_MINUTES ||
       utc_offset_minutes > INTERNET_TIME_OFFSET_MAX_MINUTES) {
        return false;
    }
    if(utc_offset_minutes % INTERNET_TIME_OFFSET_STEP_MINUTES != 0) {
        return false;
    }
    return true;
}

static int32_t internet_time_normalize_day_seconds(int32_t seconds) {
    seconds %= INTERNET_TIME_DAY_SECONDS;
    if(seconds < 0) {
        seconds += INTERNET_TIME_DAY_SECONDS;
    }
    return seconds;
}

uint16_t internet_time_beats_from_bmt_seconds(int32_t bmt_seconds) {
    int32_t normalized = internet_time_normalize_day_seconds(bmt_seconds);
    /* floor(seconds * 1000 / 86400) using integer arithmetic. */
    return (uint16_t)((normalized * (int32_t)INTERNET_TIME_BEATS_PER_DAY) /
                      (int32_t)INTERNET_TIME_DAY_SECONDS);
}

uint16_t internet_time_beats_from_local(
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    int16_t utc_offset_minutes) {
    int32_t local_seconds = ((int32_t)hour * 3600) + ((int32_t)minute * 60) + (int32_t)second;
    /* UTC = local - offset; BMT = UTC + 1 hour. */
    int32_t bmt_seconds = local_seconds - ((int32_t)utc_offset_minutes * 60) + 3600;
    return internet_time_beats_from_bmt_seconds(bmt_seconds);
}
