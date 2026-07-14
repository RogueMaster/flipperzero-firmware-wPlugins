#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "views/scheduler_start_view_settings.h"

typedef struct Scheduler Scheduler;

Scheduler* scheduler_alloc();
void scheduler_free(Scheduler* scheduler);

void scheduler_full_reset(Scheduler* scheduler);
void scheduler_time_reset(Scheduler* scheduler);
void scheduler_reset_previous_time(Scheduler* scheduler);

bool scheduler_time_to_trigger(Scheduler* scheduler);

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

uint32_t scheduler_get_previous_time(Scheduler* scheduler);
