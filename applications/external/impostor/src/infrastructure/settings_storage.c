#include "include/infrastructure/settings_storage.h"
#include <furi.h>
#include <storage/storage.h>

#define APPS_DATA_DIR "/ext/apps_data/impostor_game"
#define SETTINGS_PATH APPS_DATA_DIR "/settings.bin"

#define SETTINGS_MAGIC 0x53455431u

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t locale;
  uint8_t reserved[2];
} SettingsFile;

bool settings_load_locale(ImpostorLocale *locale_out) {
  if (!locale_out) {
    return false;
  }
  *locale_out = ImpostorLocaleEn;

  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *file = storage_file_alloc(storage);
  bool ok = false;

  if (storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
    SettingsFile f;
    if (storage_file_read(file, &f, sizeof(f)) == sizeof(f)) {
      if (f.magic == SETTINGS_MAGIC && f.version == 1) {
        if (f.locale == ImpostorLocaleEs) {
          *locale_out = ImpostorLocaleEs;
        } else {
          *locale_out = ImpostorLocaleEn;
        }
        ok = true;
      }
    }
  }

  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
  return ok;
}

void settings_save_locale(ImpostorLocale locale) {
  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *file = storage_file_alloc(storage);

  storage_common_mkdir(storage, APPS_DATA_DIR);
  if (storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
    const SettingsFile f = {
        .magic = SETTINGS_MAGIC,
        .version = 1,
        .locale = (uint8_t)(locale == ImpostorLocaleEs ? ImpostorLocaleEs
                                                       : ImpostorLocaleEn),
        .reserved = {0},
    };
    storage_file_write(file, &f, sizeof(f));
  }

  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
}
