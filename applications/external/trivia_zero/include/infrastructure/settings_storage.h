#pragma once

#include "include/domain/category.h" /* for Lang */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    Lang lang;
    uint32_t last_id;
    bool last_id_valid;
} Settings;

/* ---- Pure parsing (host-testable) ---- */

Settings settings_default(void);

/* Splits a single settings line into key + value.
 * Returns false for empty, comment (#...), whitespace-only, or lines without '='.
 * Strips leading/trailing whitespace and a single trailing '\n'. */
bool settings_split_line(const char *line, char *key, size_t key_size, char *value,
                         size_t value_size);

/* Applies a known key=value pair to settings. Returns true if the key was
 * recognized AND the value was valid. Unknown keys / invalid values return
 * false and leave settings unchanged. */
bool settings_apply_kv(Settings *s, const char *key, const char *value);

/* ---- Furi I/O (hardware-only; declared here, verified on device) ---- */

bool settings_load(Settings *out);
bool settings_save(const Settings *s);
