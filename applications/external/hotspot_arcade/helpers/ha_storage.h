#pragma once

#include <furi.h>

typedef struct HotspotArcadeApp HotspotArcadeApp;

void ha_storage_ensure_dirs(void);

void ha_storage_load_config(HotspotArcadeApp* app);
void ha_storage_save_config(HotspotArcadeApp* app);

// Read a file (text or binary) into `out`, capped at `cap` bytes. Binary-safe.
// Returns false on error/empty.
bool ha_storage_read_file(const char* path, FuriString* out, size_t cap);

// Parse manifest.json into app->assets[] / asset_count, preferring a user bundle in
// apps_data over the one bundled in the fap, and recording the winner in app->web_dir.
// Returns false if neither manifest is present or has no entries.
bool ha_storage_load_manifest(HotspotArcadeApp* app);

void ha_timestamp(FuriString* out);
