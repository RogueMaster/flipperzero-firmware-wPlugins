#pragma once

#include "sr_settings.h"

/*
 * Device-side settings storage. It includes furi / storage, so it is not a ★ module.
 * The pure logic (structure / defaults / validation / serialization / parsing) lives in
 * sr_settings; this layer only does SD I/O plus atomic replacement via a temp file and rename.
 *
 * For every load return value, *out satisfies sr_settings_is_valid(out) (for non-NULL out).
 */

enum { SR_SETTINGS_STORE_MAX = 512 };

typedef enum {
    SrSettingsLoadOk = 0, /* File exists, header matches, no illegal values, no malformed lines */
    SrSettingsLoadDegraded, /* Header matches but there were illegal values or malformed lines -> values clamped to legal */
    SrSettingsLoadMissing, /* File does not exist */
    SrSettingsLoadUnreadable, /* Exists but failed to open/read, or length is 0 or over the limit */
    SrSettingsLoadBadHeader, /* Content was read but the header does not match */
} SrSettingsLoadStatus;

SrSettingsLoadStatus sr_settings_store_load(SrSettings* out, SrSettingsParseStats* stats);

/* Atomic replacement. true = the settings file now holds the contents of *s. false = the old file was left untouched. */
bool sr_settings_store_save(const SrSettings* s);

const char* sr_settings_store_status_str(SrSettingsLoadStatus st);
