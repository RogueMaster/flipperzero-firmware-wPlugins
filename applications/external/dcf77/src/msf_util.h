#ifndef MSF_UTIL_H
#define MSF_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MSF_FRAME_SECONDS 60U

void set_msf_timecode(
    bool* bits_a,
    bool* bits_b,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t weekday,
    bool summer_time_active,
    bool summer_time_warning,
    int8_t dut1_tenths);

#endif
