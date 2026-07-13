#pragma once

#include "period_tracker_models.h"
#include <storage/storage.h>

// Force our own APP_DATA_PATH definition
// The build system may be providing an incorrect one
#ifdef APP_DATA_PATH
#undef APP_DATA_PATH
#endif
#define APP_DATA_PATH(path) "/ext/apps_data/period_tracker/" path

// CSV field escaping
void csv_escape_field(const char* input, char* output, size_t output_size);
bool csv_unescape_field(const char* input, char* output, size_t output_size);

// Profile CSV functions
bool profile_save(Storage* storage, const GirlProfile* profile);
bool profile_load(Storage* storage, const char* name, GirlProfile* profile);
// Note: profile_delete() exists in .c file but is not exposed to prevent accidental data loss
// Use profile_archive() instead to preserve data while hiding profiles
bool profile_archive(Storage* storage, const char* name);
bool profile_exists(Storage* storage, const char* name);
bool profile_list_all(
    Storage* storage,
    char names[][MAX_NAME_LENGTH],
    uint8_t* count,
    uint8_t max_count);

// Event CSV functions
bool events_load_all(
    Storage* storage,
    const char* girl_name,
    CycleEvent* events,
    uint16_t* count,
    uint16_t max_events);
bool event_append(Storage* storage, const char* girl_name, const CycleEvent* event);
bool events_get_last_period_start(Storage* storage, const char* girl_name, SimpleDate* date);

// Settings functions
bool settings_save(Storage* storage, const AppSettings* settings);
bool settings_load(Storage* storage, AppSettings* settings);

// Helper to build file paths
void build_profile_path(const char* name, char* path, size_t path_size);
void build_events_path(const char* name, char* path, size_t path_size);
void build_archive_profile_path(const char* name, char* path, size_t path_size);
