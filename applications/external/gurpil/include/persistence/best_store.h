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
 * Returns: the stored best distance, or 0 if the file is missing. A file that exists but is
 * too short, corrupt (bad magic/version), or unreadable is never treated as "0 and free to
 * overwrite": it is renamed aside to the first free "best.dat.bad".."best.dat.bad9" before
 * defaulting to 0, so a later save can't erase it. Never crashes.
 */
int32_t best_store_load(void);

/* Saves `best` as the persisted best distance, creating or overwriting the app's data file.
 *
 * Silently no-ops on any storage failure (open/write error), and for the rest of this
 * session after a load found unreadable data that couldn't be quarantined (every backup
 * slot taken, or the rename itself failed) — persistence is best-effort and must never
 * crash, block gameplay, or erase the only copy left.
 */
void best_store_save(int32_t best);
