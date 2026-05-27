#pragma once

#include "stock.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Guard against corrupt huge files exhausting heap on Flipper. */
#define STOCK_DB_MAX_BYTES (256U * 1024U)

bool fs_write_replace(const char *path, const void *data, size_t len);

bool fs_read_exact(const char *path, void *buf, size_t len);

bool fs_append_bytes(const char *path, const void *data, size_t len);

bool fs_read_stock_at(const char *path, uint32_t index, StockItem *out);

bool fs_delete_stock_at_index(const char *path, uint32_t index);

bool fs_find_stock_by_uid(const char *filepath, const uint8_t *uid,
                          uint8_t uid_len, StockItem *found_item);

bool fs_read_all_stock_items(const char *path, StockItem **out_items,
                             size_t *out_count);
