#ifndef TIMED_REMOTE_TEST_BUILD
#include <furi_hal.h>
#include <stdio.h>
#endif

#include "time_helper.h"

uint32_t time_hms_sec(uint8_t h, uint8_t m, uint8_t s) {
    return (uint32_t)h * 3600 + (uint32_t)m * 60 + (uint32_t)s;
}

void time_sec_hms(uint32_t total_seconds, uint8_t* h, uint8_t* m, uint8_t* s) {
    /* Wrap values >= 24 hours into a single day. */
    total_seconds %= 24 * 3600;

    *h = (uint8_t)(total_seconds / 3600);
    total_seconds %= 3600;
    *m = (uint8_t)(total_seconds / 60);
    *s = (uint8_t)(total_seconds % 60);
}

#ifndef TIMED_REMOTE_TEST_BUILD
uint32_t time_until(uint8_t target_h, uint8_t target_m, uint8_t target_s) {
    DateTime now;
    uint32_t now_seconds;
    uint32_t target_seconds;

    furi_hal_rtc_get_datetime(&now);

    now_seconds = time_hms_sec(now.hour, now.minute, now.second);
    target_seconds = time_hms_sec(target_h, target_m, target_s);

    if(target_seconds < now_seconds) return (24 * 3600 - now_seconds) + target_seconds;

    return target_seconds - now_seconds;
}

void time_name(char* buffer, size_t buffer_size) {
    DateTime now;

    furi_hal_rtc_get_datetime(&now);

    snprintf(
        buffer,
        buffer_size,
        "IR_%04d%02d%02d_%02d%02d%02d",
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second);
}
#endif
