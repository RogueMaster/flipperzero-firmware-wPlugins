#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sr_types.h"

/*
 * ★ Pure-logic layer for settings. Must not include any furi header (ADR-003 / ADR-012).
 *
 * Structure, defaults, validation, clamping, text serialization, bounded-scan
 * deserialization, and stealth-mode effective-value derivation. Device-side
 * storage I/O lives in sr_settings_store (T1.2b); this file touches no storage_* / furi_*.
 *
 * The on-disk surface syntax follows the Flipper File Format (per the header
 * comment of the local SDK's flipper_format.h, 2026-08-16): separator is ": "
 * (colon + one space), # starts a comment, Filetype: + Version: header, writes LF,
 * tolerates a trailing CR on read. This is not parsing by the official library.
 *
 * The baud rate table is a list of UI choices, not an assertion that the SDK
 * supports them. The device side filters separately with
 * furi_hal_serial_is_baud_rate_supported() (T1.2b).
 *
 * parse input contract (ADR-012 decision 5, same as ADR-010):
 *   Only [buf, buf+len) is guaranteed readable; do not assume NUL termination.
 *   The implementation must not use strlen / strchr / strcmp / strstr /
 *   strtoul / atoi / sscanf on the input buffer, and must not copy a line into
 *   a fixed stack buffer.
 *   Scan strictly within the len bound and memcmp only length-known fragments.
 *
 * stealth is an independent stored bit; it does not rewrite sound / vibro /
 * backlight_always. Effective behavior goes through sr_settings_effective_*().
 */

enum {
    SR_SETTINGS_VERSION = 1,
    SR_SETTINGS_TEXT_MAX = 160, /* Serialization buffer size, including the trailing NUL slot */
    SR_SETTINGS_BAUD_CHOICES = 6,
    SR_SETTINGS_SOURCE_CHOICES = 2
};

typedef struct {
    uint32_t baud;
    SrSourceKind source; /* SrSourceUnknown = auto-probe; SrSourceNative is illegal */
    bool sound;
    bool vibro;
    bool backlight_always;
    bool stealth;
    /*
     * Whether the Dash tab draws the two debug rows (d=/f= and the heap probe).
     * Defaults to false.
     * Affects drawing only, never capture or serial behavior -- a pure presentation
     * bit, like stealth.
     * NOTE: rx= does NOT belong here. It is the user-visible reading that tells
     *    whether the link is alive (the D10 defect was witnessed through it), and
     *    classifying it as debug would invalidate several existing on-device SOPs.
     *    See ADR-024.
     */
    bool debug_rows;
} SrSettings;

typedef struct {
    bool header_ok;
    uint32_t lines_seen; /* Non-empty non-comment lines, including the two header lines */
    uint32_t keys_known; /* Recognized with a valid value (a repeated key counts each time) */
    uint32_t keys_unknown;
    uint32_t values_invalid;
    uint32_t lines_malformed; /* Non-empty, non-comment, but missing ": " */
} SrSettingsParseStats;

void sr_settings_defaults(SrSettings* out);
bool sr_settings_is_valid(const SrSettings* s);
void sr_settings_clamp(SrSettings* s); /* Per field: illegal -> fall back to the default */

/*
 * On success returns the bytes written (excluding the trailing NUL) and guarantees
 * NUL termination.
 * Insufficient cap -> returns 0, and sets out[0] = '\0' when cap > 0
 * (all-or-nothing, because the device side writes this buffer straight to disk).
 */
size_t sr_settings_serialize(const SrSettings* s, char* out, size_t cap);

/*
 * Only [buf, buf+len) is guaranteed readable. Returns true only when the header matches;
 * on false, *out is already all defaults. stats may be NULL.
 * Before returning, sr_settings_is_valid(out) is guaranteed true (for non-NULL out).
 */
bool sr_settings_parse(
    const char* buf,
    size_t len,
    SrSettings* out,
    SrSettingsParseStats* stats);

bool sr_settings_effective_sound(const SrSettings* s);
bool sr_settings_effective_vibro(const SrSettings* s);
bool sr_settings_effective_backlight(const SrSettings* s);

uint32_t sr_settings_baud_choice(size_t idx); /* Out of range returns 0 */
size_t sr_settings_baud_index(uint32_t baud); /* Not in the table returns SR_SETTINGS_BAUD_CHOICES */

SrSourceKind sr_settings_source_choice(size_t idx); /* Out of range returns SrSourceUnknown */
size_t sr_settings_source_index(SrSourceKind k); /* Not in the table returns SR_SETTINGS_SOURCE_CHOICES */
bool sr_settings_equal(const SrSettings* a, const SrSettings* b);
