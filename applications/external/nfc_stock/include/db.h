#pragma once
#include "stock.h"
#include <stdbool.h>
#include <stdint.h>

bool stock_db_find_by_uid(const char *filepath, const uint8_t *uid,
                          uint8_t uid_len, StockItem *found_item);

/** Insert or replace by UID; preserves other rows in the DB file. */
bool stock_db_upsert(const char *filepath, const StockItem *item);
