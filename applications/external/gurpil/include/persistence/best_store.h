#pragma once

#include <stdint.h>

/*
 * The furi isolation boundary for best-distance persistence: `src/persistence/best_store.c`
 * talks to Flipper's Storage/File HAL, but this header stays plain C so the app layer can
 * depend on it without ever including furi. Reuses the pure record codec
 * (`include/domain/record.h`) for the byte format; this module only does the file I/O.
 */

/* Loads the persisted best distance from the app's data file.
 *
 * Returns: the stored best distance, or 0 if the file is missing, too short, corrupt
 * (bad magic/version), or any storage error occurs. Never crashes.
 */
int32_t best_store_load(void);

/* Saves `best` as the persisted best distance, creating or overwriting the app's data file.
 *
 * Silently no-ops on any storage failure (open/write error) — persistence is best-effort
 * and must never crash or block gameplay.
 */
void best_store_save(int32_t best);
