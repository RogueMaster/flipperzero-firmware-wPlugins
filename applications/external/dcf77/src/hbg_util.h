#ifndef HBG_UTIL_H
#define HBG_UTIL_H

#include <stdint.h>

#include "radio_clock_protocol.h"

typedef enum {
    HbgStartPatternMinute = 2U,
    HbgStartPatternHour = 3U,
    HbgStartPatternNoon = 4U,
} HbgStartPattern;

HbgStartPattern hbg_get_start_pattern(uint8_t hour, uint8_t minute);
void hbg_build_start_waveform(RadioClockSecondWaveform* waveform, HbgStartPattern pattern);

#endif
