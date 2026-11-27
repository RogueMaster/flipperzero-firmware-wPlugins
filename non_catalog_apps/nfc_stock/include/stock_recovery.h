#pragma once

#include "stock.h"
#include "storage_port.h"
#include "store_status.h"
#include <stdbool.h>
#include <stddef.h>

/* app->active_db_path is 128 bytes; room for the longest ".badN"/".tmp" suffix
 * and '\0'. */
#define STOCK_PATH_MAX 160

typedef struct {
    StoreLoadStatus status; /* Ok / Missing / Corrupt / IoError */
    bool save_blocked; /* true when the caller must refuse the write */
    bool quarantine_slots_full; /* meaningful only when save_blocked && status ==
                               * StoreLoadCorrupt; false there means a free slot
                               * existed but the rename into it failed. */
    char kept_path[STOCK_PATH_MAX]; /* set only when a corrupt file was
                                     quarantined */
} StockLoadResult;

typedef enum {
    StockWriteOk = 0,
    StockWriteOkQuarantined,
    StockWriteBlockedSlotsFull,
    StockWriteBlockedQuarantineFailed,
    StockWriteBlockedIoError,
    StockWriteRefusedOverLimit, /* deliberate policy refusal: the DB is fine, this
                               * write would just make it too large -- never
                               * attempted */
    StockWriteFailed,
} StockWriteOutcome;

typedef struct {
    StockWriteOutcome outcome;
    /* Set whenever a corrupt file was quarantined during this call, even if the
   * outcome is StockWriteFailed -- an earlier quarantine is a real,
   * already-committed disk change and must not be dropped just because the new
   * save itself then failed. */
    char kept_path[STOCK_PATH_MAX];
} StockWriteResult;

/**
 * Loads `main_path` through `port`, recovering from `<main_path>.tmp` when the
 * main file is missing or unreadable (a crashed save can leave the rename
 * half-done), and quarantining it to the first free ".bad" slot when it's
 * present but doesn't parse. `*out_items`/
 * `*out_count` describe what a save should build on: the recovered content on
 * `StoreLoadOk`, or an empty DB otherwise -- callers must still check
 * `save_blocked` before writing anything, since "empty" here does not mean
 * "safe to overwrite".
 */
StockLoadResult stock_recovery_load(
    const StockStoragePort* port,
    const char* main_path,
    StockItem** out_items,
    size_t* out_count);

/**
 * Loads, inserts-or-replaces `item` by uid, and saves -- unless the load
 * reports `save_blocked`, in which case nothing is written and the DB is left
 * exactly as found.
 */
StockWriteResult stock_recovery_upsert(
    const StockStoragePort* port,
    const char* main_path,
    const StockItem* item);

/**
 * Loads, removes the record at `index`, and saves -- unless the load reports
 * `save_blocked`, or `index` is out of range for the loaded DB.
 */
StockWriteResult
    stock_recovery_delete_at(const StockStoragePort* port, const char* main_path, size_t index);
