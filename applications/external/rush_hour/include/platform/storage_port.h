#pragma once
#include "../persistence/progress.h"
#include <stdbool.h>

// Load progress from /ext/apps_data/tutu/progress.bin into *p.
// Returns false if the file is missing (caller should use tutu_progress_default). A file that
// exists but is short, unreadable, or fails an I/O check is never treated as "missing and free
// to overwrite": it is renamed aside to the first free "progress.bin.bad".."progress.bin.bad9"
// before this returns false.
bool tutu_storage_load_progress(TutuProgress* p);

// Save *p to /ext/apps_data/tutu/progress.bin. Returns true on success; false on any storage
// failure, and for the rest of this session after a load found unreadable data that couldn't
// be quarantined (every backup slot taken, or the rename itself failed).
bool tutu_storage_save_progress(const TutuProgress* p);
