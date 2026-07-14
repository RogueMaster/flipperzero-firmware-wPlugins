#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t h;
    uint8_t m;
    uint8_t s;
} SchedulerHMS;

void scheduler_seconds_to_hms_string(uint32_t seconds, char* out, size_t out_size);

void scheduler_seconds_to_hms(uint32_t seconds, SchedulerHMS* time);
uint32_t scheduler_hms_to_seconds(SchedulerHMS* time);
