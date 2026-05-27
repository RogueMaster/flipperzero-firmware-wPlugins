/**
 * Single-record file helpers (tests / legacy). Multi-item DB uses stock_db_* in
 * db.c.
 */
#include "include/fs_compat.h"
#include "include/stock.h"

bool stock_item_save(const StockItem *item, const char *filepath) {
  if (!item || !filepath)
    return false;
  return fs_write_replace(filepath, item, sizeof(StockItem));
}

bool stock_item_load(StockItem *item, const char *filepath) {
  if (!item || !filepath)
    return false;
  return fs_read_exact(filepath, item, sizeof(StockItem));
}
