#pragma once

#include <stdbool.h>

#define AVOCADO_STORE_BACKUP_SLOTS 10

typedef enum {
    AvocadoStoreLoadOk = 0,
    AvocadoStoreLoadMissing,
    AvocadoStoreLoadCorrupt,
} AvocadoStoreLoadStatus;

// Only a clean load may autosave at launch; a Missing or Corrupt load must never be persisted
// over until a user action explicitly saves.
bool avocado_store_autosave_allowed(AvocadoStoreLoadStatus status);

// First free backup slot ("<file>.bad", then ".bad1".."<file>.bad9"), so a later corruption
// never overwrites an earlier kept copy. taken[i] is true if slot i's file already exists.
// Returns -1 if every slot is taken.
int avocado_store_backup_slot(const bool taken[AVOCADO_STORE_BACKUP_SLOTS]);
