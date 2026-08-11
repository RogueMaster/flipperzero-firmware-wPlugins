/*
 * Just enough JSON to read a preset file.
 *
 * Presets are flat objects of strings, numbers and booleans, so a full parser
 * would be a liability rather than a feature: more code, more to go wrong, and
 * nothing here needs nesting or arrays. This finds a key at the top level and
 * hands back its raw value token.
 *
 * Free of furi so the host tests can cover it — the interesting cases are all
 * malformed input, which is exactly what a file on a user's SD card will be
 * sooner or later.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Copy the value of `key` into `out`. Strings arrive unquoted; numbers and
 *  booleans arrive verbatim. False if the key is absent or the value does not
 *  fit. Only top-level keys are considered. */
bool meshcore_json_get(const char* json, const char* key, char* out, size_t cap);

/** Read a decimal number, scaled to an integer without ever touching a float.
 *
 * `scale` is the multiplier, so "868.731018" with scale 1000 yields 868731 —
 * the same truncation the reference client performs, which matters because the
 * wire only carries whole kHz.
 *
 * False on anything that is not a plain decimal number.
 */
bool meshcore_parse_scaled(const char* text, uint32_t scale, uint32_t* out);

/** Read an unsigned integer key. False if absent or not a number. */
bool meshcore_json_get_uint(const char* json, const char* key, uint32_t* out);

/** Read a decimal key and scale it, e.g. MHz -> kHz with scale 1000. */
bool meshcore_json_get_scaled(const char* json, const char* key, uint32_t scale, uint32_t* out);

/** Read a boolean key. False if absent or not true/false. */
bool meshcore_json_get_bool(const char* json, const char* key, bool* out);
