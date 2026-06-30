#include <stdint.h>
#include <stdbool.h>

#include "scenes/scheduler_scene_settings.h"

typedef struct Scheduler Scheduler;

Scheduler* scheduler_alloc();
void scheduler_free(Scheduler* scheduler);

void scheduler_reset(Scheduler* scheduler);
void scheduler_reset_previous_time(Scheduler* scheduler);

bool scheduler_time_to_trigger(Scheduler* scheduler);
void scheduler_get_countdown_fmt(Scheduler* scheduler, char* buffer, uint8_t size);

void scheduler_set_interval(Scheduler* scheduler, uint8_t interval);
void scheduler_set_timing_mode(Scheduler* scheduler, bool mode);
void scheduler_set_tx_repeats(Scheduler* scheduler, uint8_t tx_repeats);
void scheduler_set_mode(Scheduler* scheduler, SchedulerTxMode mode);
void scheduler_set_tx_delay(Scheduler* scheduler, uint8_t tx_delay);
void scheduler_set_file(Scheduler* scheduler, const char* file_name, int8_t list_count);

uint8_t scheduler_get_interval(Scheduler* scheduler);
uint8_t scheduler_get_tx_repeats(Scheduler* scheduler);
SchedulerTxMode scheduler_get_mode(Scheduler* scheduler);
uint16_t scheduler_get_tx_delay(Scheduler* scheduler);
FileTxType scheduler_get_file_type(Scheduler* scheduler);

const char* scheduler_get_file_name(Scheduler* scheduler);
uint32_t scheduler_get_previous_time(Scheduler* scheduler);

uint8_t scheduler_get_tx_delay_index(Scheduler* scheduler);
uint8_t scheduler_get_list_count(Scheduler* scheduler);
bool scheduler_get_timing_mode(Scheduler* scheduler);