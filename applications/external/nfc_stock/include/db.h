#pragma once
#include "stock.h"
#include "stock_recovery.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool stock_db_find_by_uid(
    const char* filepath,
    const uint8_t* uid,
    uint8_t uid_len,
    StockItem* found_item);

/**
 * Insert or replace by UID; preserves other rows in the DB file. Recovers a
 * crashed save from `<filepath>.tmp` and quarantines a present-but-unreadable
 * DB to `<filepath>.bad*` before writing anything -- see stock_recovery_upsert
 * for the full contract.
 */
StockWriteResult stock_db_upsert(const char* filepath, const StockItem* item);

/** Removes the record at `index`; same recovery/quarantine contract as
 * stock_db_upsert. */
StockWriteResult stock_db_delete_at(const char* filepath, size_t index);
