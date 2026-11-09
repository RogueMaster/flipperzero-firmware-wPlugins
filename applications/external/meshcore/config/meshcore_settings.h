/*
 * User feedback settings — sound, vibro and the RGB LED, each independently
 * switchable. Default is all on; the field may want the LED only (quiet but
 * visible), vibro only (silent and dark), or everything off (night on the river,
 * battery saving). Persisted to the SD card so the choice survives a restart.
 */
#pragma once

#include <stdbool.h>

typedef struct {
    bool sound;
    bool vibro;
    bool led;
} MeshCoreSettings;

/** Load into `out`; any missing/short file yields the all-on default. */
void meshcore_settings_load(MeshCoreSettings* out);

/** Persist. Best-effort: a missing card just means the choice does not stick. */
void meshcore_settings_save(const MeshCoreSettings* settings);
