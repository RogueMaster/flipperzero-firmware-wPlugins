#include "hbg_util.h"

#define HBG_SLOT_TICKS ((32768U + 5U) / 10U)

HbgStartPattern hbg_get_start_pattern(uint8_t hour, uint8_t minute) {
    if(minute != 0U) {
        return HbgStartPatternMinute;
    }

    if(hour == 0U || hour == 12U) {
        return HbgStartPatternNoon;
    }

    return HbgStartPatternHour;
}

void hbg_build_start_waveform(RadioClockSecondWaveform* waveform, HbgStartPattern pattern) {
    uint8_t pulse_count = (uint8_t)pattern;
    uint16_t consumed_ticks = 0U;

    waveform->segment_count = 0U;

    for(uint8_t i = 0; i < pulse_count; i++) {
        waveform->segments[waveform->segment_count].level = false;
        waveform->segments[waveform->segment_count].ticks = HBG_SLOT_TICKS;
        waveform->segment_count++;
        consumed_ticks = (uint16_t)(consumed_ticks + HBG_SLOT_TICKS);

        if(i + 1U < pulse_count) {
            waveform->segments[waveform->segment_count].level = true;
            waveform->segments[waveform->segment_count].ticks = HBG_SLOT_TICKS;
            waveform->segment_count++;
            consumed_ticks = (uint16_t)(consumed_ticks + HBG_SLOT_TICKS);
        }
    }

    waveform->segments[waveform->segment_count].level = true;
    waveform->segments[waveform->segment_count].ticks = (uint16_t)(32768U - consumed_ticks);
    waveform->segment_count++;

    for(uint8_t i = waveform->segment_count; i < RADIO_CLOCK_MAX_SECOND_SEGMENTS; i++) {
        waveform->segments[i].level = false;
        waveform->segments[i].ticks = 0U;
    }
}
