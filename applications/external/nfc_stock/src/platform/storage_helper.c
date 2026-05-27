#ifndef HOST_BUILD
#include "include/storage_helper.h"
#include <storage/storage.h>
#include <string.h>

void storage_helper_ensure_default() {
  Storage *storage = furi_record_open(RECORD_STORAGE);

  storage_common_mkdir(storage, STORAGE_PATH);

  if (!storage_common_exists(storage, CONFIG_FILE)) {
    File *file = storage_file_alloc(storage);
    if (storage_file_open(file, CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
      const char *def = DEFAULT_DB_PATH;
      storage_file_write(file, def, strlen(def));
      storage_file_close(file);
    }
    storage_file_free(file);
  }

  furi_record_close(RECORD_STORAGE);
}

bool storage_helper_get_active_db(char *path, size_t size) {
  Storage *storage = furi_record_open(RECORD_STORAGE);
  bool success = false;

  File *file = storage_file_alloc(storage);
  if (storage_file_open(file, CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
    size_t read = storage_file_read(file, path, size - 1);
    path[read] = '\0';
    success = true;
    storage_file_close(file);
  }
  storage_file_free(file);

  furi_record_close(RECORD_STORAGE);
  if (!success) {
    strncpy(path, DEFAULT_DB_PATH, size);
    path[size - 1] = '\0';
  }
  return success;
}

bool storage_helper_set_active_db(const char *path) {
  Storage *storage = furi_record_open(RECORD_STORAGE);
  bool success = false;

  File *file = storage_file_alloc(storage);
  if (storage_file_open(file, CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
    storage_file_write(file, path, strlen(path));
    success = true;
    storage_file_close(file);
  }
  storage_file_free(file);

  furi_record_close(RECORD_STORAGE);
  return success;
}
#else
/* Host test mocks */
#include "include/storage_helper.h"
#include <string.h>

static char mock_db_path[128];
bool storage_helper_get_active_db(char *path, size_t size) {
  strncpy(path, mock_db_path, size);
  return true;
}
bool storage_helper_set_active_db(const char *path) {
  strncpy(mock_db_path, path, sizeof(mock_db_path));
  return true;
}
void storage_helper_ensure_default() {}
#endif
