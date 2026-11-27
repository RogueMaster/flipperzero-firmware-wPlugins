#pragma once

#include "stock.h"
#include "storage_port.h"
#include "store_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Guard against corrupt huge files exhausting heap on Flipper. */
#define STOCK_DB_MAX_BYTES (256U * 1024U)

bool fs_write_replace(const char* path, const void* data, size_t len);

bool fs_read_exact(const char* path, void* buf, size_t len);

bool fs_append_bytes(const char* path, const void* data, size_t len);

bool fs_read_stock_at(const char* path, uint32_t index, StockItem* out);

bool fs_find_stock_by_uid(
    const char* filepath,
    const uint8_t* uid,
    uint8_t uid_len,
    StockItem* found_item);

bool fs_read_all_stock_items(const char* path, StockItem** out_items, size_t* out_count);

/** Same as fs_read_all_stock_items, but distinguishes a genuinely corrupt size
 * (StockReadFraming), a well-formed size over STOCK_DB_MAX_BYTES
 * (StockReadTooLarge), and a read/allocation failure the caller can't trust
 * either way (StockReadBlocked). */
StockReadOutcome
    fs_read_all_stock_items_ex(const char* path, StockItem** out_items, size_t* out_count);

/** Only StoreStatMissing is a genuine "file does not exist" (FSE_NOT_EXIST). */
StoreStat fs_stat(const char* path);

bool fs_exists(const char* path);

/** Wraps storage_common_rename, which is NOT atomic on this firmware: it
 * removes `dst`, copies `src` to `dst`, then removes `src`. A crash between
 * those steps can leave `dst` missing or partially written while `src` is still
 * intact. */
bool fs_rename(const char* src, const char* dst);

bool fs_remove(const char* path);

/** Reads whatever bytes are at `path` with no framing validation, up to
 * STOCK_DB_MAX_BYTES. False only on a genuine open/read failure or an
 * over-cap size; an empty file is true with `*out_len == 0`. */
bool fs_read_raw_bytes(const char* path, uint8_t** out_bytes, size_t* out_len);
