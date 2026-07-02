#ifndef BPC_UTIL_H
#define BPC_UTIL_H

#include <stdint.h>

#include "radio_clock_pulse.h"

void set_bpc_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year_since_2000,
    uint8_t weekday);

#endif
