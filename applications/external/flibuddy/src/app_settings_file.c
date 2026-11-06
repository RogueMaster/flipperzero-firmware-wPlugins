#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include "app_settings_file.h"

static bool file_load(void* ctx, uint8_t version, void* data, size_t size) {
    UNUSED(ctx);
    return saved_struct_load(APP_SETTINGS_PATH, data, size, APP_SETTINGS_MAGIC, version);
}

static bool file_save(void* ctx, const void* data, size_t size) {
    UNUSED(ctx);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* mkdir creates one level; true when the directory already exists */
    bool dir = storage_simply_mkdir(storage, APP_SETTINGS_PARENT) &&
               storage_simply_mkdir(storage, APP_SETTINGS_DIR);
    furi_record_close(RECORD_STORAGE);
    return dir && saved_struct_save(
                      APP_SETTINGS_PATH, data, size, APP_SETTINGS_MAGIC, APP_SETTINGS_VERSION);
}

const AppSettingsStorage app_settings_file = {
    .load = file_load,
    .save = file_save,
    .ctx = NULL,
};
