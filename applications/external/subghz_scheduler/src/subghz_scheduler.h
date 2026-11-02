#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "views/scheduler_start_view_settings.h"

typedef struct Scheduler Scheduler;

Scheduler* scheduler_alloc();
void scheduler_free(Scheduler* scheduler);

void scheduler_full_reset(Scheduler* scheduler);
void scheduler_time_reset(Scheduler* scheduler);

// Run control. Timing is based on the RTC, the GUI tick only polls.
void scheduler_start(Scheduler* scheduler);
bool scheduler_time_to_trigger(Scheduler* scheduler);
int16_t scheduler_fire(Scheduler* scheduler);
void scheduler_tx_done(Scheduler* scheduler);
bool scheduler_one_shot_done(Scheduler* scheduler);
void scheduler_set_embedded_timing(Scheduler* scheduler, bool enabled);
bool scheduler_is_armed(Scheduler* scheduler);
uint8_t scheduler_get_current_line(Scheduler* scheduler);

void scheduler_set_interval_seconds(Scheduler* scheduler, uint32_t interval_seconds);
uint32_t scheduler_get_interval_seconds(Scheduler* scheduler);

uint32_t scheduler_get_countdown_seconds(Scheduler* scheduler);

void scheduler_set_timing_mode(Scheduler* scheduler, bool timing_mode);
bool scheduler_get_timing_mode(Scheduler* scheduler);

void scheduler_set_tx_count(Scheduler* scheduler, uint8_t tx_count);
uint8_t scheduler_get_tx_count(Scheduler* scheduler);

void scheduler_set_tx_mode(Scheduler* scheduler, SchedulerTxMode tx_mode);
SchedulerTxMode scheduler_get_tx_mode(Scheduler* scheduler);

void scheduler_set_tx_delay_ms(Scheduler* scheduler, uint16_t tx_delay_ms);
uint16_t scheduler_get_tx_delay_ms(Scheduler* scheduler);
uint8_t scheduler_get_tx_delay_index(Scheduler* scheduler);

void scheduler_set_radio(Scheduler* scheduler, uint8_t radio);
bool scheduler_get_radio(Scheduler* scheduler);

void scheduler_set_file(Scheduler* scheduler, const char* file_name, int8_t list_count);
const char* scheduler_get_file_name(Scheduler* scheduler);

uint8_t scheduler_get_list_count(Scheduler* scheduler);
FileTxType scheduler_get_file_type(Scheduler* scheduler);

void scheduler_set_step_mode(Scheduler* scheduler, SchedulerStepMode step_mode);
SchedulerStepMode scheduler_get_step_mode(Scheduler* scheduler);

void scheduler_set_schedule_type(Scheduler* scheduler, SchedulerScheduleType schedule_type);
SchedulerScheduleType scheduler_get_schedule_type(Scheduler* scheduler);

void scheduler_set_start_line(Scheduler* scheduler, SchedulerStartLine start_line);
SchedulerStartLine scheduler_get_start_line(Scheduler* scheduler);

// True when a playlist is sent one line per trigger
bool scheduler_is_stepping(Scheduler* scheduler);
// Schedule type in effect: anything other than Uniform requires stepping
SchedulerScheduleType scheduler_get_effective_schedule(Scheduler* scheduler);

void scheduler_set_line_interval(Scheduler* scheduler, uint8_t line, uint32_t seconds);
uint32_t scheduler_get_line_interval(Scheduler* scheduler, uint8_t line);

// Time of day in seconds since midnight
void scheduler_set_line_time(Scheduler* scheduler, uint8_t line, uint32_t seconds);
uint32_t scheduler_get_line_time(Scheduler* scheduler, uint8_t line);
