#include "include/db.h"
#include "include/fs_compat.h"
#include <stdlib.h>
#include <string.h>

bool stock_db_find_by_uid(const char *filepath, const uint8_t *uid,
                          uint8_t uid_len, StockItem *found_item) {
  return fs_find_stock_by_uid(filepath, uid, uid_len, found_item);
}

bool stock_db_upsert(const char *filepath, const StockItem *item) {
  if (!filepath || !item) {
    return false;
  }
  StockItem *items = NULL;
  size_t count = 0;
  if (!fs_read_all_stock_items(filepath, &items, &count)) {
    return fs_write_replace(filepath, item, sizeof(StockItem));
  }
  size_t idx = (size_t)-1;
  for (size_t i = 0; i < count; i++) {
    if (items[i].uid_len == item->uid_len &&
        memcmp(items[i].uid, item->uid, item->uid_len) == 0) {
      idx = i;
      break;
    }
  }
  StockItem *block = items;
  size_t n = count;
  if (idx == (size_t)-1) {
    StockItem *grown = realloc(items, (count + 1) * sizeof(StockItem));
    if (!grown) {
      free(items);
      return false;
    }
    block = grown;
    block[count] = *item;
    n = count + 1;
  } else {
    block[idx] = *item;
  }
  bool ok = fs_write_replace(filepath, block, n * sizeof(StockItem));
  free(block);
  return ok;
}
