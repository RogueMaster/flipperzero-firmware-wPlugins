#pragma once

#include <stdbool.h>

typedef struct SchedulerApp SchedulerApp;

const char* path_basename(const char* path);

bool scheduler_settings_save_to_path(SchedulerApp* app, const char* full_path);
bool scheduler_settings_load_from_path(SchedulerApp* app, const char* full_path);
