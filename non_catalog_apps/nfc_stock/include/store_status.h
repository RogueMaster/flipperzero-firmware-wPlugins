#pragma once

#include <stdbool.h>

#define STORE_BACKUP_SLOTS 10

typedef enum {
    StoreStatOk = 0,
    StoreStatMissing,
    StoreStatError,
} StoreStat;

typedef enum {
    StoreLoadOk = 0,
    StoreLoadMissing,
    StoreLoadCorrupt,
    StoreLoadIoError,
} StoreLoadStatus;

/**
 * Picks the first free backup slot ("<file>.bad", then ".bad1".."bad9") so a
 * second corruption never overwrites an earlier kept copy. taken[i] must be
 * true if slot i is already occupied on disk. Returns the slot index, or -1 if
 * every slot is taken, meaning the caller must leave the unreadable file in
 * place instead of destroying one of them.
 */
int store_status_backup_slot(const bool taken[STORE_BACKUP_SLOTS]);
