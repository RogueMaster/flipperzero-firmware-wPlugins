#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct SchedulerApp SchedulerApp;
typedef struct SchedulerIntervalView SchedulerIntervalView;

SchedulerIntervalView* scheduler_interval_view_alloc();
void scheduler_interval_view_free(SchedulerIntervalView* v);
View* scheduler_interval_view_get_view(SchedulerIntervalView* v);

void scheduler_interval_view_set_seconds(SchedulerIntervalView* v, uint32_t sec);
uint32_t scheduler_interval_view_get_seconds(SchedulerIntervalView* v);

void scheduler_interval_view_set_app(SchedulerIntervalView* v, SchedulerApp* app);
