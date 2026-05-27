#ifdef HOST_BUILD
/* Expose truncate(2) in unistd.h with glibc when using -std=c11. */
#define _DEFAULT_SOURCE 1
#endif

#include "include/fs_compat.h"
#include "include/clock.h"
#include <stdlib.h>
#include <string.h>

#ifdef HOST_BUILD
#include <stdio.h>
#include <time.h>
#include <unistd.h>

uint32_t app_now_timestamp(void) { return (uint32_t)time(NULL); }

bool fs_write_replace(const char *path, const void *data, size_t len) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;
  size_t w = fwrite(data, 1, len, f);
  fclose(f);
  return w == len;
}

bool fs_read_exact(const char *path, void *buf, size_t len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  size_t r = fread(buf, 1, len, f);
  fclose(f);
  return r == len;
}

bool fs_append_bytes(const char *path, const void *data, size_t len) {
  FILE *f = fopen(path, "ab");
  if (!f)
    return false;
  size_t w = fwrite(data, 1, len, f);
  fclose(f);
  return w == len;
}

bool fs_read_stock_at(const char *path, uint32_t index, StockItem *out) {
  FILE *f = fopen(path, "rb");
  if (!f || !out)
    return false;
  if (fseek(f, (long)(index * sizeof(StockItem)), SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  size_t r = fread(out, sizeof(StockItem), 1, f);
  fclose(f);
  return r == 1;
}

bool fs_delete_stock_at_index(const char *path, uint32_t index) {
  FILE *f = fopen(path, "r+b");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  long count = size / (long)sizeof(StockItem);
  if (index >= (uint32_t)count) {
    fclose(f);
    return false;
  }
  for (uint32_t i = index + 1; i < (uint32_t)count; i++) {
    StockItem item;
    fseek(f, (long)(i * sizeof(StockItem)), SEEK_SET);
    if (fread(&item, sizeof(StockItem), 1, f) != 1)
      break;
    fseek(f, (long)((i - 1) * sizeof(StockItem)), SEEK_SET);
    fwrite(&item, sizeof(StockItem), 1, f);
  }
  fclose(f);
  return truncate(path, (count - 1) * (long)sizeof(StockItem)) == 0;
}

bool fs_find_stock_by_uid(const char *filepath, const uint8_t *uid,
                          uint8_t uid_len, StockItem *found_item) {
  if (!filepath || !uid || !found_item)
    return false;
  FILE *f = fopen(filepath, "rb");
  if (!f)
    return false;
  StockItem temp;
  bool found = false;
  while (fread(&temp, sizeof(StockItem), 1, f) == 1) {
    if (temp.uid_len == uid_len && memcmp(temp.uid, uid, uid_len) == 0) {
      *found_item = temp;
      found = true;
      break;
    }
  }
  fclose(f);
  return found;
}

bool fs_read_all_stock_items(const char *path, StockItem **out_items,
                             size_t *out_count) {
  if (!out_items || !out_count)
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  if (sz < 0 || (size_t)sz % sizeof(StockItem) != 0) {
    fclose(f);
    return false;
  }
  if ((size_t)sz > STOCK_DB_MAX_BYTES) {
    fclose(f);
    return false;
  }
  size_t n = (size_t)sz / sizeof(StockItem);
  if (n == 0) {
    fclose(f);
    *out_items = NULL;
    *out_count = 0;
    return true;
  }
  StockItem *buf = malloc((size_t)sz);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t r = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (r != (size_t)sz) {
    free(buf);
    return false;
  }
  *out_items = buf;
  *out_count = n;
  return true;
}

#else

#include <furi.h>
#include <storage/storage.h>

uint32_t app_now_timestamp(void) { return furi_get_tick(); }

static bool open_file(File **file, Storage **storage, const char *path,
                      FS_AccessMode am, FS_OpenMode om) {
  *storage = furi_record_open(RECORD_STORAGE);
  *file = storage_file_alloc(*storage);
  if (!storage_file_open(*file, path, am, om)) {
    storage_file_free(*file);
    furi_record_close(RECORD_STORAGE);
    *file = NULL;
    *storage = NULL;
    return false;
  }
  return true;
}

static void close_file(File *file, Storage *storage) {
  (void)storage;
  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
}

bool fs_write_replace(const char *path, const void *data, size_t len) {
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    return false;
  size_t w = storage_file_write(file, data, len);
  close_file(file, storage);
  return w == len;
}

bool fs_read_exact(const char *path, void *buf, size_t len) {
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING))
    return false;
  size_t r = storage_file_read(file, buf, len);
  close_file(file, storage);
  return r == len;
}

bool fs_append_bytes(const char *path, const void *data, size_t len) {
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_WRITE, FSOM_OPEN_APPEND))
    return false;
  size_t w = storage_file_write(file, data, len);
  close_file(file, storage);
  return w == len;
}

bool fs_read_stock_at(const char *path, uint32_t index, StockItem *out) {
  if (!out)
    return false;
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING))
    return false;
  if (!storage_file_seek(file, (uint32_t)(index * sizeof(StockItem)), true)) {
    close_file(file, storage);
    return false;
  }
  size_t r = storage_file_read(file, out, sizeof(StockItem));
  close_file(file, storage);
  return r == sizeof(StockItem);
}

bool fs_delete_stock_at_index(const char *path, uint32_t index) {
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING))
    return false;
  uint64_t sz = storage_file_size(file);
  if (sz > STOCK_DB_MAX_BYTES) {
    close_file(file, storage);
    return false;
  }
  if (sz % sizeof(StockItem) != 0) {
    close_file(file, storage);
    return false;
  }
  uint32_t count = (uint32_t)(sz / sizeof(StockItem));
  if (index >= count) {
    close_file(file, storage);
    return false;
  }
  StockItem *items = malloc((size_t)sz);
  if (!items) {
    close_file(file, storage);
    return false;
  }
  storage_file_seek(file, 0, true);
  storage_file_read(file, items, (size_t)sz);
  memmove(&items[index], &items[index + 1],
          (size_t)(count - index - 1) * sizeof(StockItem));
  storage_file_seek(file, 0, true);
  size_t new_bytes = (size_t)(count - 1) * sizeof(StockItem);
  storage_file_write(file, items, new_bytes);
  storage_file_seek(file, (uint32_t)new_bytes, true);
  storage_file_truncate(file);
  free(items);
  close_file(file, storage);
  return true;
}

bool fs_find_stock_by_uid(const char *filepath, const uint8_t *uid,
                          uint8_t uid_len, StockItem *found_item) {
  if (!filepath || !uid || !found_item)
    return false;
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, filepath, FSAM_READ, FSOM_OPEN_EXISTING))
    return false;
  StockItem temp;
  bool found = false;
  while (storage_file_read(file, &temp, sizeof(StockItem)) ==
         sizeof(StockItem)) {
    if (temp.uid_len == uid_len && memcmp(temp.uid, uid, uid_len) == 0) {
      *found_item = temp;
      found = true;
      break;
    }
  }
  close_file(file, storage);
  return found;
}

bool fs_read_all_stock_items(const char *path, StockItem **out_items,
                             size_t *out_count) {
  if (!out_items || !out_count)
    return false;
  Storage *storage = NULL;
  File *file = NULL;
  if (!open_file(&file, &storage, path, FSAM_READ, FSOM_OPEN_EXISTING))
    return false;
  uint64_t sz = storage_file_size(file);
  if (sz % sizeof(StockItem) != 0) {
    close_file(file, storage);
    return false;
  }
  if (sz > STOCK_DB_MAX_BYTES) {
    close_file(file, storage);
    return false;
  }
  size_t n = (size_t)(sz / sizeof(StockItem));
  if (n == 0) {
    close_file(file, storage);
    *out_items = NULL;
    *out_count = 0;
    return true;
  }
  StockItem *buf = malloc((size_t)sz);
  if (!buf) {
    close_file(file, storage);
    return false;
  }
  storage_file_seek(file, 0, true);
  size_t r = storage_file_read(file, buf, (size_t)sz);
  close_file(file, storage);
  if (r != (size_t)sz) {
    free(buf);
    return false;
  }
  *out_items = buf;
  *out_count = n;
  return true;
}

#endif
