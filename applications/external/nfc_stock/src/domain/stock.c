/**
 * Domain: StockItem invariants and mutations. No Flipper or filesystem
 * includes.
 */
#include "include/stock.h"
#include "include/clock.h"
#include <string.h>

bool stock_item_init(StockItem *item, const char *name, int32_t quantity,
                     int32_t min_quantity, const char *location) {
  if (!item || !name)
    return false;

  strncpy(item->name, name, MAX_ITEM_NAME - 1);
  item->name[MAX_ITEM_NAME - 1] = '\0';
  item->quantity = (quantity < 0) ? 0 : quantity;
  item->min_quantity = min_quantity;
  strncpy(item->location, location, MAX_LOCATION - 1);
  item->location[MAX_LOCATION - 1] = '\0';
  item->last_updated = app_now_timestamp();

  return true;
}

void stock_item_update_quantity(StockItem *item, int32_t change) {
  if (!item)
    return;

  if (item->quantity + change < 0) {
    item->quantity = 0;
  } else {
    item->quantity += change;
  }
  item->last_updated = app_now_timestamp();
}

void stock_item_move(StockItem *item, const char *new_location) {
  if (!item || !new_location)
    return;

  strncpy(item->location, new_location, MAX_LOCATION - 1);
  item->location[MAX_LOCATION - 1] = '\0';
  item->last_updated = app_now_timestamp();
}
