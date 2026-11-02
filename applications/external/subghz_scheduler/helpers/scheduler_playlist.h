#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "views/scheduler_start_view_settings.h"

// Number of `sub:` entries in a playlist, or -1 if the file can't be opened
int scheduler_playlist_count(Storage* storage, const char* playlist_path);

typedef struct {
    uint8_t count;
    bool has_timing;
    uint32_t wait_seconds[SCHEDULER_MAX_LINES];
} SchedulerPlaylistTiming;

#define SCHEDULER_PLAYLIST_MAX_WAIT_SECONDS 359999UL

// Read ordered time/sub entries. A time applies to the next sub; following
// subs without a time are sent back to back. Returns false for invalid files.
bool scheduler_playlist_read_timing(
    Storage* storage,
    const char* playlist_path,
    SchedulerPlaylistTiming* timing);
