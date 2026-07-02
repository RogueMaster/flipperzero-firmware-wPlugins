#include "experimental_time.h"
#include <stddef.h>

static uint8_t dcf77_experimental_time_clamp_u8(uint8_t value, uint8_t min, uint8_t max) {
    if(value < min) {
        return min;
    }

    if(value > max) {
        return max;
    }

    return value;
}

void dcf77_experimental_time_normalize_datetime(
    DateTime* datetime,
    const DateTime* fallback_datetime) {
    if(datetime == NULL || fallback_datetime == NULL) {
        return;
    }

    if(datetime->year < 2000U || datetime->year > 2099U || datetime->month == 0U ||
       datetime->month > 12U || datetime->day == 0U || datetime->day > 31U) {
        *datetime = *fallback_datetime;
        return;
    }

    datetime->hour = dcf77_experimental_time_clamp_u8(datetime->hour, 0U, 23U);
    datetime->minute = dcf77_experimental_time_clamp_u8(datetime->minute, 0U, 59U);
    datetime->second = dcf77_experimental_time_clamp_u8(datetime->second, 0U, 59U);

    const bool leap_year = datetime_is_leap_year(datetime->year);
    const uint8_t max_day = datetime_get_days_per_month(leap_year, datetime->month);

    if(datetime->day > max_day) {
        datetime->day = max_day;
    }

    const uint32_t timestamp = datetime_datetime_to_timestamp(datetime);
    datetime_timestamp_to_datetime(timestamp, datetime);
}

void dcf77_experimental_time_reset_settings(
    Dcf77ExperimentalTimeSettings* settings,
    const DateTime* fallback_datetime) {
    if(settings == NULL || fallback_datetime == NULL) {
        return;
    }

    settings->enabled = false;
    settings->source = Dcf77ExperimentalTimeSourceFlipper;
    settings->preset_datetime = *fallback_datetime;
    settings->direction = Dcf77ExperimentalTimeDirectionForward;
    settings->speedup = 1U;
    settings->slowdown = 1U;
}

void dcf77_experimental_time_normalize_settings(
    Dcf77ExperimentalTimeSettings* settings,
    const DateTime* fallback_datetime) {
    if(settings == NULL || fallback_datetime == NULL) {
        return;
    }

    if(settings->source >= Dcf77ExperimentalTimeSourceCount) {
        settings->source = Dcf77ExperimentalTimeSourceFlipper;
    }

    if(settings->direction >= Dcf77ExperimentalTimeDirectionCount) {
        settings->direction = Dcf77ExperimentalTimeDirectionForward;
    }

    settings->speedup = dcf77_experimental_time_clamp_u8(settings->speedup, 1U, 60U);
    settings->slowdown = dcf77_experimental_time_clamp_u8(settings->slowdown, 1U, 60U);

    if(settings->speedup > 1U) {
        settings->slowdown = 1U;
    } else if(settings->slowdown > 1U) {
        settings->speedup = 1U;
    }

    dcf77_experimental_time_normalize_datetime(&settings->preset_datetime, fallback_datetime);
}

void dcf77_experimental_time_seed_runtime(
    Dcf77ExperimentalTimeRuntime* runtime,
    const Dcf77ExperimentalTimeSettings* settings,
    const DateTime* rtc_snapshot) {
    if(runtime == NULL || settings == NULL || rtc_snapshot == NULL) {
        return;
    }

    runtime->frame_index = 0U;

    if(settings->enabled && settings->source == Dcf77ExperimentalTimeSourcePreset) {
        runtime->base_datetime = settings->preset_datetime;
    } else {
        runtime->base_datetime = *rtc_snapshot;
    }
}

void dcf77_experimental_time_advance_runtime(Dcf77ExperimentalTimeRuntime* runtime) {
    if(runtime == NULL) {
        return;
    }

    runtime->frame_index++;
}

uint32_t dcf77_experimental_time_get_minute_delta(
    const Dcf77ExperimentalTimeSettings* settings,
    uint32_t frame_index) {
    if(settings == NULL || !settings->enabled ||
       settings->direction == Dcf77ExperimentalTimeDirectionStopped) {
        return 0U;
    }

    if(settings->speedup > 1U) {
        return frame_index * settings->speedup;
    }

    if(settings->slowdown > 1U) {
        return frame_index / settings->slowdown;
    }

    return frame_index;
}

void dcf77_experimental_time_get_frame_datetime(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    uint32_t frame_index,
    DateTime* datetime) {
    if(settings == NULL || runtime == NULL || datetime == NULL) {
        return;
    }

    const uint32_t minute_delta = dcf77_experimental_time_get_minute_delta(settings, frame_index);
    DateTime frame_datetime = runtime->base_datetime;
    uint32_t timestamp = datetime_datetime_to_timestamp(&frame_datetime);

    if(settings->enabled && settings->direction == Dcf77ExperimentalTimeDirectionBackwards) {
        const uint32_t second_delta = minute_delta * 60U;
        timestamp = (timestamp > second_delta) ? (timestamp - second_delta) : 0U;
    } else {
        timestamp += minute_delta * 60U;
    }

    datetime_timestamp_to_datetime(timestamp, &frame_datetime);
    *datetime = frame_datetime;
}

void dcf77_experimental_time_get_current_frame_datetime(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    DateTime* datetime) {
    dcf77_experimental_time_get_frame_datetime(settings, runtime, runtime->frame_index, datetime);
}

void dcf77_experimental_time_get_current_frame_minute(
    const Dcf77ExperimentalTimeSettings* settings,
    const Dcf77ExperimentalTimeRuntime* runtime,
    DateTime* datetime) {
    dcf77_experimental_time_get_current_frame_datetime(settings, runtime, datetime);

    if(datetime != NULL) {
        datetime->second = 0U;
    }
}

uint8_t dcf77_experimental_time_get_start_second(const Dcf77ExperimentalTimeRuntime* runtime) {
    if(runtime == NULL) {
        return 0U;
    }

    return runtime->base_datetime.second;
}
