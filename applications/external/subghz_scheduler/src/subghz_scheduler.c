#include "subghz_scheduler.h"
#include <furi_hal.h>

#define TAG "SubGHzScheduler"

struct Scheduler {
    uint32_t previous_run_time;
    uint32_t countdown;

    FileTxType file_type;
    SchedulerTxMode tx_mode;
    SchedulerTimingMode timing_mode;

    char* file_name;

    uint16_t tx_delay_ms;

    uint32_t interval_seconds;

    uint8_t tx_count;
    uint8_t list_count;

    bool radio;
};

Scheduler* scheduler_alloc() {
    Scheduler* scheduler = calloc(1, sizeof(Scheduler));
    furi_assert(scheduler);
    scheduler_full_reset(scheduler);
    return scheduler;
}

void scheduler_free(Scheduler* scheduler) {
    furi_assert(scheduler);
    if(scheduler->file_name) {
        free(scheduler->file_name);
        scheduler->file_name = NULL;
    }
    free(scheduler);
}

void scheduler_time_reset(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler->previous_run_time = 0;
    scheduler->countdown = 0;
}

void scheduler_full_reset(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler_time_reset(scheduler);
    scheduler->tx_delay_ms = 100;
    scheduler->interval_seconds = 10; // Still default to 10 sec?
    scheduler->tx_count = 0;
    scheduler->file_type = SchedulerFileTypeSingle;
    scheduler->list_count = 1;
    scheduler->file_name = NULL;
    scheduler->tx_mode = SchedulerTxModeNormal;
    scheduler->timing_mode = SchedulerTimingModeRelative;
    scheduler->radio = 0; // internal by default
}

void scheduler_reset_previous_time(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler->previous_run_time = furi_hal_rtc_get_timestamp();
}

void scheduler_set_interval_seconds(Scheduler* scheduler, uint32_t interval_seconds) {
    furi_assert(scheduler);

    scheduler->interval_seconds = interval_seconds ? interval_seconds : 1;
}

void scheduler_set_timing_mode(Scheduler* scheduler, bool tx_mode) {
    furi_assert(scheduler);
    scheduler->timing_mode = tx_mode;
}

void scheduler_set_tx_count(Scheduler* scheduler, uint8_t tx_count) {
    furi_assert(scheduler);
    scheduler->tx_count = tx_count;
}

void scheduler_set_tx_mode(Scheduler* scheduler, SchedulerTxMode tx_mode) {
    furi_assert(scheduler);
    scheduler->tx_mode = tx_mode;
}

void scheduler_set_tx_delay_ms(Scheduler* scheduler, uint16_t tx_delay_ms) {
    furi_assert(scheduler);
    scheduler->tx_delay_ms = CLAMP(tx_delay_ms, 1000, 0);
}

void scheduler_set_radio(Scheduler* scheduler, uint8_t radio) {
    furi_assert(scheduler);
    scheduler->radio = radio;
}

static const char* extract_filename(const char* filepath) {
    const char* filename = strrchr(filepath, '/');
    return (filename != NULL) ? (filename + 1) : filepath;
}

void scheduler_set_file(Scheduler* scheduler, const char* file_name, int8_t list_count) {
    furi_assert(scheduler);
    furi_assert(file_name);
    const char* base = extract_filename(file_name);

    // Free old name if it exists
    if(scheduler->file_name) {
        free(scheduler->file_name);
        scheduler->file_name = NULL;
    }

    scheduler->file_name = strdup(base);

    if(list_count <= 0) {
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
    uint32_t interval = (scheduler->interval_seconds > 0) ? (scheduler->interval_seconds - 1) : 0;

    if((scheduler->tx_mode != SchedulerTxModeImmediate) && !scheduler->previous_run_time) {
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

uint32_t scheduler_get_previous_time(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->previous_run_time;
}

uint32_t scheduler_get_interval_seconds(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->interval_seconds;
}

uint32_t scheduler_get_countdown_seconds(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->countdown;
}

uint8_t scheduler_get_tx_count(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_count;
}

const char* scheduler_get_file_name(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_name;
}

FileTxType scheduler_get_file_type(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_type;
}

SchedulerTxMode scheduler_get_tx_mode(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_mode;
}

uint16_t scheduler_get_tx_delay_ms(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_delay_ms;
}

bool scheduler_get_radio(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->radio;
}

uint8_t scheduler_get_tx_delay_index(Scheduler* scheduler) {
    furi_assert(scheduler);
    return (scheduler->tx_delay_ms / TX_DELAY_STEP_MS);
}

uint8_t scheduler_get_list_count(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->list_count;
}

bool scheduler_get_timing_mode(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->timing_mode;
}
