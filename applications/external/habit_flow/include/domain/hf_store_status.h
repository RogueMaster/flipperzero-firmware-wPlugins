#pragma once

#include <stdbool.h>

#define HF_STORE_BACKUP_SLOTS 10

typedef enum {
    HfStoreLoadOk = 0,
    HfStoreLoadMissing,
    HfStoreLoadCorrupt,
    HfStoreLoadStatError,
} HfStoreLoadStatus;

// Only a clean load may autosave; launch must never persist over data that was missing,
// corrupt, or impossible to check.
bool hf_store_autosave_allowed(HfStoreLoadStatus status);

// First free backup slot ("<file>.bad", then ".bad1" .. ".bad9"), so a later corruption
// never overwrites an earlier kept copy. taken[i] is true if slot i is already occupied.
// Returns -1 if every slot is taken.
int hf_store_backup_slot(const bool taken[HF_STORE_BACKUP_SLOTS]);
