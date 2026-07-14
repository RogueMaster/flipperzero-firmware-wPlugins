#pragma once

#include <gui/view.h>

typedef struct Scheduler Scheduler;
typedef struct SchedulerRunView SchedulerRunView;

SchedulerRunView* scheduler_run_view_alloc();
void scheduler_run_view_free(SchedulerRunView* run_view);
View* scheduler_run_view_get_view(SchedulerRunView* run_view);

uint8_t scheduler_run_view_get_tick_counter(SchedulerRunView* run_view);
void scheduler_run_view_inc_tick_counter(SchedulerRunView* run_view);

void scheduler_run_view_set_static_fields(SchedulerRunView* run_view, Scheduler* scheduler);
void scheduler_run_view_update_countdown(SchedulerRunView* run_view, Scheduler* scheduler);
