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

// Frankendraw artwork. The ESP streams one finished sheet as BEGIN, a frame per line
// segment, then END (see HA_MSG_ART); these turn that stream into one SVG file per
// sheet under HA_ART_DIR, written incrementally so nothing is buffered. ha_art_abort
// closes a half-written file when a session ends or a stream is interrupted.
void ha_art_begin(HotspotArcadeApp* app, const char* json);
void ha_art_stroke(HotspotArcadeApp* app, const char* json);
void ha_art_end(HotspotArcadeApp* app);
void ha_art_abort(HotspotArcadeApp* app);
