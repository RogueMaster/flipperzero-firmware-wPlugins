#pragma once

#include <furi.h>

typedef struct HotspotArcadeApp HotspotArcadeApp;

void ha_storage_ensure_dirs(void);

void ha_storage_load_config(HotspotArcadeApp* app);
void ha_storage_save_config(HotspotArcadeApp* app);

// Read a file (text or binary) into `out`, capped at `cap` bytes. Binary-safe.
// Returns false on error/empty.
bool ha_storage_read_file(const char* path, FuriString* out, size_t cap);

// Parse manifest.json (HA_WEB_DIR) into app->assets[] / asset_count.
// Returns false if the manifest is missing or has no entries.
bool ha_storage_load_manifest(HotspotArcadeApp* app);

void ha_timestamp(FuriString* out);
