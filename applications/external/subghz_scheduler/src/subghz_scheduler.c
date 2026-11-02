#include "subghz_scheduler.h"
#include <furi_hal.h>

#define TAG "SubGHzScheduler"

struct Scheduler {
    uint32_t next_fire_time;
    bool armed;
    bool embedded_timing;

    FileTxType file_type;
    SchedulerTxMode tx_mode;
    SchedulerTimingMode timing_mode;
    SchedulerStepMode step_mode;
    SchedulerScheduleType schedule_type;
    SchedulerStartLine start_line;

    char* file_name;

    uint16_t tx_delay_ms;

    uint32_t interval_seconds;

    uint8_t tx_count;
    uint8_t list_count;
    uint8_t current_line;

    bool radio;

    uint32_t line_interval[SCHEDULER_MAX_LINES];
    uint32_t line_time[SCHEDULER_MAX_LINES]; // seconds since midnight
};

static void scheduler_reset_lines(Scheduler* scheduler) {
    for(uint8_t i = 0; i < SCHEDULER_MAX_LINES; ++i) {
        scheduler->line_interval[i] = scheduler->interval_seconds;
        scheduler->line_time[i] = DEFAULT_LINE_SECONDS;
    }
}

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
    scheduler->next_fire_time = 0;
    scheduler->armed = false;
    scheduler->current_line = 0;
}

void scheduler_full_reset(Scheduler* scheduler) {
    furi_assert(scheduler);
    scheduler_time_reset(scheduler);
    scheduler->tx_delay_ms = 100;
    scheduler->interval_seconds = 10; // Still default to 10 sec?
    scheduler->tx_count = 0;
    scheduler->file_type = SchedulerFileTypeSingle;
    scheduler->list_count = 1;
    if(scheduler->file_name) {
        free(scheduler->file_name);
    }
    scheduler->file_name = NULL;
    scheduler->tx_mode = SchedulerTxModeNormal;
    scheduler->timing_mode = SchedulerTimingModeRelative;
    scheduler->step_mode = SchedulerStepModeAll;
    scheduler->schedule_type = SchedulerScheduleUniform;
    scheduler->start_line = SchedulerStartLineFirst;
    scheduler->embedded_timing = false;
    scheduler->radio = 0; // internal by default
    scheduler_reset_lines(scheduler);
}

bool scheduler_is_stepping(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_type == SchedulerFileTypePlaylist &&
           scheduler->step_mode == SchedulerStepModeOne;
}

SchedulerScheduleType scheduler_get_effective_schedule(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler_is_stepping(scheduler) ? scheduler->schedule_type : SchedulerScheduleUniform;
}

// Next time line fires at its time of day, counted from base
static uint32_t scheduler_time_of_day_fire(
    Scheduler* scheduler,
    uint8_t line,
    uint32_t base,
    bool allow_equal) {
    // RTC holds local time, so the timestamp modulo a day is the time of day
    uint32_t fire = base - (base % SECONDS_PER_DAY) + scheduler->line_time[line];
    if(fire < base || (fire == base && !allow_equal)) {
        fire += SECONDS_PER_DAY;
    }
    return fire;
}

// Next fire time for the current line, counted from base.
// allow_equal: a time of day equal to base fires at base instead of the next day, so lines
// sharing a time fire back to back.
static uint32_t scheduler_next_fire_from(Scheduler* scheduler, uint32_t base, bool allow_equal) {
    const uint8_t line = scheduler->current_line;

    switch(scheduler_get_effective_schedule(scheduler)) {
    case SchedulerSchedulePerLine:
        return base + scheduler->line_interval[line];
    case SchedulerScheduleTimeOfDay:
        return scheduler_time_of_day_fire(scheduler, line, base, allow_equal);
    case SchedulerScheduleUniform:
    default:
        return base + scheduler->interval_seconds;
    }
}

// Line whose time of day comes up first after now, the lowest line wins a tie
static uint8_t scheduler_nearest_line(Scheduler* scheduler, uint32_t now) {
    uint8_t nearest = 0;
    uint32_t nearest_fire = UINT32_MAX;

    for(uint8_t i = 0; i < scheduler->list_count; ++i) {
        const uint32_t fire = scheduler_time_of_day_fire(scheduler, i, now, false);
        if(fire < nearest_fire) {
            nearest = i;
            nearest_fire = fire;
        }
    }
    return nearest;
}

void scheduler_start(Scheduler* scheduler) {
    furi_assert(scheduler);
    const uint32_t now = furi_hal_rtc_get_timestamp();

    scheduler->current_line = 0;
    if(scheduler_get_effective_schedule(scheduler) == SchedulerScheduleTimeOfDay &&
       scheduler->start_line == SchedulerStartLineNearest) {
        scheduler->current_line = scheduler_nearest_line(scheduler, now);
    }
    if(scheduler->tx_mode == SchedulerTxModeImmediate) {
        scheduler->next_fire_time = now;
    } else {
        scheduler->next_fire_time = scheduler_next_fire_from(scheduler, now, false);
    }
    scheduler->armed = true;
}

bool scheduler_time_to_trigger(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->armed && furi_hal_rtc_get_timestamp() >= scheduler->next_fire_time;
}

int16_t scheduler_fire(Scheduler* scheduler) {
    furi_assert(scheduler);
    const uint32_t now = furi_hal_rtc_get_timestamp();

    int16_t line = -1;
    if(scheduler_is_stepping(scheduler)) {
        line = scheduler->current_line;
        scheduler->current_line = (scheduler->current_line + 1) % scheduler->list_count;
    }

    if(scheduler_get_effective_schedule(scheduler) == SchedulerScheduleTimeOfDay) {
        // Count from the scheduled time, so a line with the same time fires right after this
        // one, and times that passed during a long transmission are not pushed a day.
        // Wrapping back to the first line starts a new day, never fire it again right away.
        const bool same_cycle = scheduler->current_line != 0;
        scheduler->next_fire_time =
            scheduler_next_fire_from(scheduler, scheduler->next_fire_time, same_cycle);
        scheduler->armed = true;
    } else if(scheduler->timing_mode == SchedulerTimingModePrecise) {
        // Count from the scheduled time to avoid drift, unless we fell behind
        uint32_t next = scheduler_next_fire_from(scheduler, scheduler->next_fire_time, false);
        if(next < now) {
            next = scheduler_next_fire_from(scheduler, now, false);
        }
        scheduler->next_fire_time = next;
        scheduler->armed = true;
    } else {
        // Relative: next interval starts when the transmission ends
        scheduler->armed = false;
    }

    return line;
}

void scheduler_tx_done(Scheduler* scheduler) {
    furi_assert(scheduler);
    if(!scheduler->armed) {
        scheduler->next_fire_time =
            scheduler_next_fire_from(scheduler, furi_hal_rtc_get_timestamp(), false);
        scheduler->armed = true;
    }
}

bool scheduler_one_shot_done(Scheduler* scheduler) {
    furi_assert(scheduler);
    if(scheduler->embedded_timing) return scheduler->current_line == 0;
    // Lines with a zero wait are chained to the line before them
    if(scheduler_get_effective_schedule(scheduler) == SchedulerSchedulePerLine &&
       scheduler->current_line != 0 && scheduler->line_interval[scheduler->current_line] == 0) {
        return false;
    }
    // Same for lines sharing the time of the line before them
    const uint8_t line = scheduler->current_line;
    if(scheduler_get_effective_schedule(scheduler) == SchedulerScheduleTimeOfDay && line != 0 &&
       scheduler->line_time[line] == scheduler->line_time[line - 1]) {
        return false;
    }
    return true;
}

void scheduler_set_embedded_timing(Scheduler* scheduler, bool enabled) {
    furi_assert(scheduler);
    scheduler->embedded_timing = enabled;
}

bool scheduler_is_armed(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->armed;
}

uint8_t scheduler_get_current_line(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->current_line;
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

void scheduler_set_step_mode(Scheduler* scheduler, SchedulerStepMode step_mode) {
    furi_assert(scheduler);
    scheduler->step_mode = (step_mode < SchedulerStepModeNum) ? step_mode : SchedulerStepModeAll;
}

void scheduler_set_schedule_type(Scheduler* scheduler, SchedulerScheduleType schedule_type) {
    furi_assert(scheduler);
    scheduler->schedule_type = (schedule_type < SchedulerScheduleNum) ? schedule_type :
                                                                        SchedulerScheduleUniform;
}

void scheduler_set_line_interval(Scheduler* scheduler, uint8_t line, uint32_t seconds) {
    furi_assert(scheduler);
    if(line < SCHEDULER_MAX_LINES) {
        scheduler->line_interval[line] = seconds;
    }
}

void scheduler_set_line_time(Scheduler* scheduler, uint8_t line, uint32_t seconds) {
    furi_assert(scheduler);
    if(line < SCHEDULER_MAX_LINES) {
        scheduler->line_time[line] = seconds % SECONDS_PER_DAY;
    }
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

    const uint8_t old_count = scheduler->list_count;
    if(list_count <= 0) {
        scheduler->file_type = SchedulerFileTypeSingle;
        scheduler->list_count = 1;
    } else {
        scheduler->file_type = SchedulerFileTypePlaylist;
        scheduler->list_count = list_count;
    }

    if(scheduler->list_count != old_count) {
        scheduler_reset_lines(scheduler);
    }
    scheduler->current_line = 0;
}

uint32_t scheduler_get_interval_seconds(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->interval_seconds;
}

uint32_t scheduler_get_countdown_seconds(Scheduler* scheduler) {
    furi_assert(scheduler);
    if(!scheduler->armed) {
        return 0;
    }
    const uint32_t now = furi_hal_rtc_get_timestamp();
    return (scheduler->next_fire_time > now) ? (scheduler->next_fire_time - now) : 0;
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

SchedulerStepMode scheduler_get_step_mode(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->step_mode;
}

SchedulerScheduleType scheduler_get_schedule_type(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->schedule_type;
}

uint32_t scheduler_get_line_interval(Scheduler* scheduler, uint8_t line) {
    furi_assert(scheduler);
    return (line < SCHEDULER_MAX_LINES) ? scheduler->line_interval[line] : 0;
}

uint32_t scheduler_get_line_time(Scheduler* scheduler, uint8_t line) {
    furi_assert(scheduler);
    return (line < SCHEDULER_MAX_LINES) ? scheduler->line_time[line] : 0;
}

void scheduler_set_start_line(Scheduler* scheduler, SchedulerStartLine start_line) {
    furi_assert(scheduler);
    scheduler->start_line = (start_line < SchedulerStartLineNum) ? start_line :
                                                                   SchedulerStartLineFirst;
}

SchedulerStartLine scheduler_get_start_line(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->start_line;
}
