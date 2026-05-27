#ifndef STOCK_H
#define STOCK_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_ITEM_NAME 32
#define MAX_LOCATION 32

typedef struct {
  uint8_t uid[10];
  uint8_t uid_len;
  char name[MAX_ITEM_NAME];
  int32_t quantity;
  int32_t min_quantity;
  char location[MAX_LOCATION];
  uint32_t last_updated;
} StockItem;

bool stock_item_init(StockItem *item, const char *name, int32_t quantity,
                     int32_t min_quantity, const char *location);
void stock_item_update_quantity(StockItem *item, int32_t change);
void stock_item_move(StockItem *item, const char *new_location);

/* Implemented in persistence/stock_file.c (single-record file I/O). */
bool stock_item_save(const StockItem *item, const char *filepath);
bool stock_item_load(StockItem *item, const char *filepath);

#endif
