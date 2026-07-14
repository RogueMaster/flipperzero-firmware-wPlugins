#ifndef DCF77_EXPERIMENTAL_TIME_H
#define DCF77_EXPERIMENTAL_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef DCF77_HOST_TEST
#include "datetime_stub.h"
#else
#include <datetime/datetime.h>
#endif

typedef enum {
    Dcf77ExperimentalTimeSourceFlipper = 0,
    Dcf77ExperimentalTimeSourcePreset,
    Dcf77ExperimentalTimeSourceCount,
} Dcf77ExperimentalTimeSource;

typedef enum {
    Dcf77ExperimentalTimeDirectionForward = 0,
    Dcf77ExperimentalTimeDirectionStopped,
    Dcf77ExperimentalTimeDirectionBackwards,
    Dcf77ExperimentalTimeDirectionCount,
} Dcf77ExperimentalTimeDirection;

typedef struct {
    bool enabled;
    Dcf77ExperimentalTimeSource source;
    DateTime preset_datetime;
    uint8_t direction;
    uint8_t speedup;
    uint8_t slowdown;
} Dcf77ExperimentalTimeSettings;

typedef struct {
    DateTime base_datetime;
    uint32_t frame_index;
} Dcf77ExperimentalTimeRuntime;

void dcf77_experimental_time_reset_settings(
    Dcf77ExperimentalTimeSettings* settings,
    const DateTime* fallback_datetime);
void dcf77_experimental_time_normalize_settings(
    Dcf77ExperimentalTimeSettings* settings,
    const DateTime* fallback_datetime);
void dcf77_experimental_time_normalize_datetime(
    DateTime* datetime,
    const DateTime* fallback_datetime);

void dcf77_experimental_time_seed_runtime(
    Dcf77ExperimentalTimeRuntime* runtime,
    const Dcf77ExperimentalTimeSettings* settings,
    const DateTime* rtc_snapshot);
void dcf77_experimental_time_advance_runtime(Dcf77ExperimentalTimeRuntime* runtime);

uint32_t dcf77_experimental_time_get_minute_delta(
    const Dcf77ExperimentalTimeSettings* settings,
    uint32_t frame_index);
void dcf77_experimental_time_get_frame_datetime(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    uint32_t frame_index,
    DateTime* datetime);
void dcf77_experimental_time_get_current_frame_datetime(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    DateTime* datetime);
void dcf77_experimental_time_get_current_frame_minute(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    DateTime* datetime);
uint8_t dcf77_experimental_time_get_start_second(const Dcf77ExperimentalTimeRuntime* runtime);

#endif
