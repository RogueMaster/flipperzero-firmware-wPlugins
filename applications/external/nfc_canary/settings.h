#pragma once

#include "nfc_canary_i.h"

/* Populate with defaults tuned for pocket carry: vibration on for Warn and
 * above, sound reserved for Alarm. */
void settings_defaults(AlerterSettings* s);

/* Load from SD, falling back to defaults if absent or version-mismatched. */
void settings_load(AlerterSettings* s);

void settings_save(const AlerterSettings* s);
