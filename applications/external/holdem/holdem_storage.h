#ifndef HOLDEM_STORAGE_H
#define HOLDEM_STORAGE_H

#include "holdem_types.h"

// Returns true when a valid save file exists on external storage.
bool save_exists_on_storage(void);

// Removes the single save slot used by the app.
void delete_save_on_storage(void);

// Writes the complete game state (version 2 format).
bool save_progress(const HoldemGame* game);

// Loads a complete game state; also accepts legacy version 1 saves.
bool load_progress(HoldemGame* game);

#endif
