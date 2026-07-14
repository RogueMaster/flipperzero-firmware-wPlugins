#include "scheduler_hms.h"

#include <stdio.h>

void scheduler_seconds_to_hms_string(uint32_t seconds, char* out, size_t out_size) {
    uint32_t h = (seconds / 3600) % 100;
    uint32_t m = (seconds / 60) % 60;
    uint32_t s = seconds % 60;

    // "HH:MM:SS" -> 8 chars + NUL
    snprintf(out, out_size, "%02lu:%02lu:%02lu", h, m, s);
}

void scheduler_seconds_to_hms(uint32_t seconds, SchedulerHMS* time) {
    time->h = (seconds / 3600) % 100;
    time->m = (seconds / 60) % 60;
    time->s = seconds % 60;
}

uint32_t scheduler_hms_to_seconds(SchedulerHMS* time) {
    return ((uint32_t)time->h * 3600) + ((uint32_t)time->m * 60) + (uint32_t)time->s;
}
