#include "zk_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define ZK_SETTINGS_MAGIC     0x47464B50U
#define ZK_SETTINGS_VERSION   1
#define ZK_SETTINGS_PATH      APP_DATA_PATH("settings.bin")
#define ZK_SETTINGS_TEMP_PATH APP_DATA_PATH("settings.tmp")

void zk_settings_init(ZkSettings* settings) {
    memset(settings, 0, sizeof(*settings));
    settings->magic = ZK_SETTINGS_MAGIC;
    settings->version = ZK_SETTINGS_VERSION;
    strlcpy(settings->default_name, "Password", sizeof(settings->default_name));
}

bool zk_settings_load(ZkSettings* settings) {
    zk_settings_init(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    const bool opened = storage_file_open(file, ZK_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    bool valid = false;
    if(opened && storage_file_size(file) == sizeof(*settings)) {
        valid = storage_file_read(file, settings, sizeof(*settings)) == sizeof(*settings) &&
                settings->magic == ZK_SETTINGS_MAGIC && settings->version == ZK_SETTINGS_VERSION &&
                settings->default_name[0] && settings->default_name[ZK_NAME_LENGTH - 1] == '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!valid) zk_settings_init(settings);
    return valid;
}

bool zk_settings_save(const ZkSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, ZK_SETTINGS_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        success = storage_file_write(file, settings, sizeof(*settings)) == sizeof(*settings) &&
                  storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(success) {
        storage_common_remove(storage, ZK_SETTINGS_PATH);
        success = storage_common_rename(storage, ZK_SETTINGS_TEMP_PATH, ZK_SETTINGS_PATH) ==
                  FSE_OK;
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}
