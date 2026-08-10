#include "clock_model.h"
#include "internet_time.h"

#include <stdio.h>

bool clock_model_format_beats(uint16_t beats, char* out, size_t out_size) {
    if(out == NULL || out_size < 5) {
        return false;
    }
    if(beats > 999) {
        beats = beats % INTERNET_TIME_BEATS_PER_DAY;
    }
    (void)snprintf(out, out_size, "@%03u", (unsigned)beats);
    return true;
}

bool clock_model_format_local_time(
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    bool hour_format_24,
    char* out,
    size_t out_size) {
    if(out == NULL) {
        return false;
    }

    if(hour_format_24) {
        if(out_size < 9) {
            return false;
        }
        (void)snprintf(
            out, out_size, "%02u:%02u:%02u", (unsigned)hour, (unsigned)minute, (unsigned)second);
        return true;
    }

    if(out_size < 12) {
        return false;
    }

    const char* ampm = (hour < 12) ? "AM" : "PM";
    unsigned display_hour = (unsigned)(hour % 12);
    if(display_hour == 0) {
        display_hour = 12;
    }
    (void)snprintf(
        out, out_size, "%02u:%02u:%02u %s", display_hour, (unsigned)minute, (unsigned)second, ampm);
    return true;
}

void clock_model_build_snapshot(const ClockModelInput* input, ClockModelSnapshot* out) {
    if(input == NULL || out == NULL) {
        return;
    }

    out->beats = internet_time_beats_from_local(
        input->hour, input->minute, input->second, input->utc_offset_minutes);
    (void)clock_model_format_beats(out->beats, out->beats_text, sizeof(out->beats_text));
    (void)clock_model_format_local_time(
        input->hour,
        input->minute,
        input->second,
        input->hour_format_24,
        out->local_time_text,
        sizeof(out->local_time_text));
}
