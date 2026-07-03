#ifndef JJY_UTIL_H
#define JJY_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_clock_pulse.h"

void set_jjy_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint16_t day_of_year,
    uint8_t year,
    uint8_t weekday,
    bool su1,
    bool su2,
    bool leap_second_pending,
    bool leap_second_insert);

#endif
