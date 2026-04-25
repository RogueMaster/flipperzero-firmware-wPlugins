#pragma once
#include "fsd_handler.h"

/**
 * prefs.h — NVS settings persistence via Arduino Preferences (ESP32 flash).
 *
 * Saves all runtime-toggleable state so the device resumes where it left off
 * after a power cycle or deep sleep wakeup.  op_mode is saved too — the user
 * activated the device deliberately and expects it back on next boot.
 *
 * On first boot (empty NVS) prefs_load() is a no-op; fsd_state_init() defaults
 * remain in effect.
 */

/** Load saved settings into *state.  No-op if no saved state exists yet. */
void prefs_load(FSDState *state);

/** Persist all runtime-toggleable fields from *state to NVS.
 *  Call after every user-initiated toggle (button or web dashboard). */
void prefs_save(const FSDState *state);

/** Erase all saved settings from NVS (factory reset). */
void prefs_clear();
