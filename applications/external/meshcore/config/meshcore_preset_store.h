/*
 * The presets available to pick from: the compiled-in ones, plus whatever is
 * on the card.
 *
 * Built-ins come first and always exist, so the Profiles screen is never empty
 * and a node can be set up with no SD card at all. Card presets follow, in
 * directory order.
 *
 * A file that does not parse is not silently skipped — it is kept with the
 * reason, and shown as a failed entry. A preset that quietly vanishes is worse
 * than one that says why it did not load, because the first thing anyone does
 * with a hand-edited JSON file is mistype a field name.
 */
#pragma once

#include <furi.h>
#include <storage/storage.h>

#include "meshcore_preset.h"

/* Built-ins plus a sensible number of files. A field kit has a handful of
 * network configurations, not fifty. */
#define MESHCORE_PRESET_STORE_MAX 24u
/* Long enough for the reason strings meshcore_preset_from_json produces. */
#define MESHCORE_PRESET_ERROR_LEN 40u

#define MESHCORE_PRESET_DIR EXT_PATH("apps_data/meshcore_cfg/presets")

typedef struct {
    MeshCorePreset preset;
    /* Empty when the entry loaded. Otherwise why it did not, ready to show. */
    char error[MESHCORE_PRESET_ERROR_LEN];
    /* File it came from, for the error line. Empty for built-ins. */
    char file[32];
} MeshCorePresetEntry;

typedef struct {
    MeshCorePresetEntry items[MESHCORE_PRESET_STORE_MAX];
    size_t count;
    /* Files found beyond the table's capacity. */
    uint32_t skipped;
    /* True once the card has been read, whatever it contained. */
    bool scanned;
} MeshCorePresetStore;

/** Fill with the built-ins only. Cannot fail. */
void meshcore_preset_store_init(MeshCorePresetStore* store);

/** Re-read: built-ins, then every *.json under MESHCORE_PRESET_DIR. Blocking
 *  (it touches the card), so call it from a worker thread. */
void meshcore_preset_store_scan(MeshCorePresetStore* store, Storage* storage);

/** True when this entry can be applied; false when it failed to load. */
bool meshcore_preset_store_usable(const MeshCorePresetEntry* entry);

/** Write a preset to the card as JSON, so a node that was set up by hand can
 *  be cloned onto the rest. False if the card would not take it. */
bool meshcore_preset_store_save(
    Storage* storage,
    const MeshCorePreset* preset,
    const char** why);
