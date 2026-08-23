#include "bv_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "BioVaultSettings"

#define SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define SETTINGS_MAGIC "BVS1"
#define SETTINGS_MAGIC_LEN 4
#define SETTINGS_SIZE (SETTINGS_MAGIC_LEN + 1) // magic + send_newline

void bv_settings_load(BvSettings* out) {
    memset(out, 0, sizeof(*out)); // defaults: everything off

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint8_t buf[SETTINGS_SIZE];
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, buf, sizeof(buf)) == sizeof(buf) &&
           memcmp(buf, SETTINGS_MAGIC, SETTINGS_MAGIC_LEN) == 0) {
            out->send_newline = buf[SETTINGS_MAGIC_LEN] != 0;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool bv_settings_save(const BvSettings* s) {
    uint8_t buf[SETTINGS_SIZE];
    memcpy(buf, SETTINGS_MAGIC, SETTINGS_MAGIC_LEN);
    buf[SETTINGS_MAGIC_LEN] = s->send_newline ? 1 : 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH("")); // ensure app data dir exists
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, buf, sizeof(buf)) == sizeof(buf);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "save settings: %s", ok ? "OK" : "FAIL");
    return ok;
}
