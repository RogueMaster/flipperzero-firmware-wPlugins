#ifndef BSF_UTIL_H
#define BSF_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_clock_pulse.h"

void set_bsf_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t weekday,
    bool leap_second_warning,
    bool dst_active);

#endif
