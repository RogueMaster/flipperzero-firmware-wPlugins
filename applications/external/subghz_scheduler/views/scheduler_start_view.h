#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct SchedulerApp SchedulerApp;

void scheduler_start_view_rebuild(SchedulerApp* app);

void scheduler_start_view_reset(SchedulerApp* app);
