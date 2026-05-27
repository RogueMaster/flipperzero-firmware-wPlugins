#include "include/db.h"
#include "include/fs_compat.h"
#include "include/stock.h"
#include "include/storage_helper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void test_init() {
  StockItem item;
  bool success = stock_item_init(&item, "Tornillo", 100, 10, "Estante A");
  assert(success == true);
  assert(strcmp(item.name, "Tornillo") == 0);
  assert(item.quantity == 100);
  assert(item.min_quantity == 10);
  assert(strcmp(item.location, "Estante A") == 0);
  printf("test_init: PASSED\n");
}

void test_update_quantity() {
  StockItem item;
  stock_item_init(&item, "Tuerca", 50, 5, "Estante B");

  stock_item_update_quantity(&item, 20);
  assert(item.quantity == 70);

  /* Quantity must not go negative */
  stock_item_update_quantity(&item, -100);
  assert(item.quantity == 0);
  printf("test_update_quantity: PASSED\n");
}

void test_move() {
  StockItem item;
  stock_item_init(&item, "Arandela", 200, 20, "Caja 1");

  stock_item_move(&item, "Caja 2");
  assert(strcmp(item.location, "Caja 2") == 0);
  printf("test_move: PASSED\n");
}

void test_persistence() {
  StockItem original;
  StockItem loaded;
  const char *path = "test_item.bin";

  stock_item_init(&original, "Persist", 10, 2, "SD Card");

  /* Save and load round-trip */
  bool saved = stock_item_save(&original, path);
  assert(saved == true);

  bool loaded_ok = stock_item_load(&loaded, path);
  assert(loaded_ok == true);

  assert(strcmp(original.name, loaded.name) == 0);
  assert(original.quantity == loaded.quantity);
  printf("test_persistence: PASSED\n");

  unlink(path);
}

void test_storage_logic() {
  char buffer[128];
  const char *test_path = "/ext/apps_data/nfc_stock_manager/test_db.bin";

  bool set = storage_helper_set_active_db(test_path);
  assert(set == true);

  bool get = storage_helper_get_active_db(buffer, sizeof(buffer));
  assert(get == true);
  assert(strcmp(buffer, test_path) == 0);
  printf("test_storage_logic: PASSED\n");
}

void test_db_lookup() {
  const char *path = "test_db.bin";
  StockItem item1, found;
  stock_item_init(&item1, "Tornillo", 10, 1, "A1");
  memcpy(item1.uid, (uint8_t[]){0x01, 0x02, 0x03}, 3);
  item1.uid_len = 3;

  FILE *f = fopen(path, "wb");
  fwrite(&item1, sizeof(StockItem), 1, f);
  fclose(f);

  bool found_ok =
      stock_db_find_by_uid(path, (uint8_t[]){0x01, 0x02, 0x03}, 3, &found);
  assert(found_ok == true);
  assert(strcmp(found.name, "Tornillo") == 0);
  printf("test_db_lookup: PASSED\n");
  unlink(path);
}

void test_db_upsert(void) {
  const char *path = "test_upsert.bin";
  unlink(path);

  StockItem a, b, loaded;
  stock_item_init(&a, "A", 1, 0, "L1");
  memcpy(a.uid, (uint8_t[]){0xAA}, 1);
  a.uid_len = 1;

  stock_item_init(&b, "B", 2, 0, "L2");
  memcpy(b.uid, (uint8_t[]){0xBB}, 1);
  b.uid_len = 1;

  assert(stock_db_upsert(path, &a) == true);
  assert(stock_db_upsert(path, &b) == true);

  a.quantity = 99;
  assert(stock_db_upsert(path, &a) == true);

  assert(stock_db_find_by_uid(path, (uint8_t[]){0xAA}, 1, &loaded) == true);
  assert(loaded.quantity == 99);
  assert(stock_db_find_by_uid(path, (uint8_t[]){0xBB}, 1, &loaded) == true);
  assert(loaded.quantity == 2);

  printf("test_db_upsert: PASSED\n");
  unlink(path);
}

void test_read_all_empty_db(void) {
  const char *path = "test_empty.bin";
  unlink(path);

  /* Create an empty DB file and ensure it is handled safely. */
  FILE *f = fopen(path, "wb");
  assert(f != NULL);
  fclose(f);

  StockItem *items = (StockItem *)0x1;
  size_t count = 123;
  bool ok = fs_read_all_stock_items(path, &items, &count);
  assert(ok == true);
  assert(items == NULL);
  assert(count == 0);

  printf("test_read_all_empty_db: PASSED\n");
  unlink(path);
}

void test_read_all_rejects_corrupt_size(void) {
  const char *path = "test_corrupt.bin";
  unlink(path);

  /* Write a non-multiple of StockItem to simulate file corruption. */
  FILE *f = fopen(path, "wb");
  assert(f != NULL);
  uint8_t bad[3] = {0xAA, 0xBB, 0xCC};
  fwrite(bad, sizeof(bad), 1, f);
  fclose(f);

  StockItem *items = NULL;
  size_t count = 0;
  bool ok = fs_read_all_stock_items(path, &items, &count);
  assert(ok == false);

  printf("test_read_all_rejects_corrupt_size: PASSED\n");
  unlink(path);
}

int main() {
  printf("Running all tests...\n");
  test_init();
  test_update_quantity();
  test_move();
  test_persistence();
  test_storage_logic();
  test_db_lookup();
  test_db_upsert();
  test_read_all_empty_db();
  test_read_all_rejects_corrupt_size();
  printf("All tests PASSED!\n");
  return 0;
}
