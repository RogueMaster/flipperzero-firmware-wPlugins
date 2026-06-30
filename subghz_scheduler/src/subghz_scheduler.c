#include "subghz_scheduler.h"
#include <furi_hal.h>

#define TAG "SubGHzScheduler"

struct Scheduler {
    uint32_t previous_run_time;
    uint32_t countdown;
    uint16_t tx_delay;
    uint8_t interval;
    uint8_t tx_repeats;
    FileTxType file_type;
    uint8_t list_count;
    char* file_name;
    SchedulerTxMode mode;
    bool timing_mode;
};

Scheduler* scheduler_alloc() {
    Scheduler* scheduler = calloc(1, sizeof(Scheduler));
    furi_assert(scheduler);

    scheduler->previous_run_time = 0;
    scheduler->countdown = 0;
    scheduler->tx_delay = SchedulerTxDelay100;
    scheduler->interval = Interval10Sec;
    scheduler->tx_repeats = 0;
    scheduler->file_type = SchedulerFileTypeSingle;
    scheduler->list_count = 1;
    scheduler->file_name = NULL;
    scheduler->mode = SchedulerTxModeNormal;
    scheduler->timing_mode = SchedulerTimingModeRelative;

    return scheduler;
}

void scheduler_free(Scheduler* scheduler) {
    furi_assert(scheduler);
    free(scheduler);
}

void scheduler_reset(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler->previous_run_time = 0;
    scheduler->countdown = 0;
}

void scheduler_reset_previous_time(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler->previous_run_time = furi_hal_rtc_get_timestamp();
}

void scheduler_set_interval(Scheduler* scheduler, uint8_t interval) {
    furi_assert(scheduler);
    scheduler->interval = interval;
    scheduler->countdown = interval_second_value[scheduler->interval];
}

void scheduler_set_timing_mode(Scheduler* scheduler, bool mode) {
    furi_assert(scheduler);
    scheduler->timing_mode = mode;
}

void scheduler_set_tx_repeats(Scheduler* scheduler, uint8_t tx_repeats) {
    furi_assert(scheduler);
    scheduler->tx_repeats = tx_repeats;
}

void scheduler_set_mode(Scheduler* scheduler, SchedulerTxMode mode) {
    furi_assert(scheduler);
    scheduler->mode = mode;
}

void scheduler_set_tx_delay(Scheduler* scheduler, uint8_t tx_delay) {
    furi_assert(scheduler);
    scheduler->tx_delay = tx_delay_value[tx_delay];
}

static const char* extract_filename(const char* filepath) {
    const char* filename = strrchr(filepath, '/');
    return (filename != NULL) ? (filename + 1) : filepath;
}

void scheduler_set_file(Scheduler* scheduler, const char* file_name, int8_t list_count) {
    furi_assert(scheduler);
    const char* name = extract_filename(file_name);
    scheduler->file_name = (char*)name;
    if(list_count == 0) {
        scheduler->file_type = SchedulerFileTypeSingle;
        scheduler->list_count = 1;
    } else {
        scheduler->file_type = SchedulerFileTypePlaylist;
        scheduler->list_count = list_count;
    }
}

bool scheduler_time_to_trigger(Scheduler* scheduler) {
    furi_assert(scheduler);
    uint32_t current_time = furi_hal_rtc_get_timestamp();
    uint32_t interval = interval_second_value[scheduler->interval] - 1; // zero index the interval

    if((scheduler->mode != SchedulerTxModeImmediate) && !scheduler->previous_run_time) {
        scheduler->previous_run_time = current_time;
        scheduler->countdown = interval;
        return false; // Don't trigger immediately
    }

    if(scheduler->countdown == 0) {
        scheduler->previous_run_time = current_time;
        scheduler->countdown = interval;
        return true;
    }
    scheduler->countdown--;
    return false;
}

void scheduler_get_countdown_fmt(Scheduler* scheduler, char* buffer, uint8_t size) {
    furi_assert(scheduler);
    snprintf(
        buffer,
        size,
        "%02lu:%02lu:%02lu",
        scheduler->countdown / 60 / 60,
        scheduler->countdown / 60 % 60,
        scheduler->countdown % 60);
}

uint32_t scheduler_get_previous_time(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->previous_run_time;
}

uint8_t scheduler_get_interval(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->interval;
}

uint8_t scheduler_get_tx_repeats(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_repeats;
}

const char* scheduler_get_file_name(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_name;
}

FileTxType scheduler_get_file_type(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_type;
}

SchedulerTxMode scheduler_get_mode(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->mode;
}

uint16_t scheduler_get_tx_delay(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_delay;
}

uint8_t scheduler_get_tx_delay_index(Scheduler* scheduler) {
    furi_assert(scheduler);
    switch(scheduler->tx_delay) {
    case SchedulerTxDelay100:
        return 0;
    case SchedulerTxDelay250:
        return 1;
    case SchedulerTxDelay500:
        return 2;
    case SchedulerTxDelay1000:
        return 3;
    default:
        return 0;
    }
}

uint8_t scheduler_get_list_count(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->list_count;
}

bool scheduler_get_timing_mode(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->timing_mode;
}
