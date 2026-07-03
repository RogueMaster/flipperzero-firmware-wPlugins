#ifndef WWVB_UTIL_H
#define WWVB_UTIL_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_clock_pulse.h"

void set_wwvb_am_symbols(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint16_t day_of_year,
    uint8_t year,
    bool dst_active,
    bool dst_recent_change,
    bool leap_year,
    bool leap_second,
    int8_t ut1_correction_tenths);

bool wwvb_is_leap_year(uint16_t year);
uint16_t wwvb_day_of_year(uint16_t year, uint8_t month, uint8_t day);

#endif
