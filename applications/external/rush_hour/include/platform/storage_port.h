#pragma once
#include "../persistence/progress.h"
#include <stdbool.h>

// Load progress from /ext/apps_data/tutu/progress.bin into *p.
// Returns false if no file / bad size (caller should use tutu_progress_default).
bool tutu_storage_load_progress(TutuProgress *p);

// Save *p to /ext/apps_data/tutu/progress.bin. Returns true on success.
bool tutu_storage_save_progress(const TutuProgress *p);
