#pragma once

/* Persist the user's Nyx settings across launches. Without this, every open
 * resets Mode / Sensitivity / probe pin, which is a small paper-cut you feel
 * every single sweep. Backed by the firmware's saved_struct helper, so a
 * version bump or a corrupt file falls back to defaults instead of loading
 * garbage. */

typedef struct NyxSettings NyxSettings;

void nyx_store_settings_save(const NyxSettings* s);
void nyx_store_settings_load(NyxSettings* s); // leaves *s untouched if none on disk
