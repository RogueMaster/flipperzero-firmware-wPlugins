#include "trident_storage.h"
#include "../trident_i.h"

#include <storage/storage.h>
#include <string.h>

#define TRIDENT_SETTINGS_DIR  "/ext/apps_data/trident"
#define TRIDENT_SETTINGS_PATH TRIDENT_SETTINGS_DIR "/trident.settings"
#define TRIDENT_SETTINGS_MAGIC 0x54524431UL // "TRD1"
#define TRIDENT_SETTINGS_VER 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    TridentSettings settings;
} TridentSettingsBlob;

void trident_settings_load(TridentSettings* settings) {
    furi_assert(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    TridentSettingsBlob blob;

    if(storage_file_open(file, TRIDENT_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t read = storage_file_read(file, &blob, sizeof(blob));
        if(read == sizeof(blob) && blob.magic == TRIDENT_SETTINGS_MAGIC &&
           blob.version == TRIDENT_SETTINGS_VER && blob.size == sizeof(TridentSettings)) {
            *settings = blob.settings;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void trident_settings_save(const TridentSettings* settings) {
    furi_assert(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    storage_common_mkdir(storage, TRIDENT_SETTINGS_DIR);

    TridentSettingsBlob blob = {
        .magic = TRIDENT_SETTINGS_MAGIC,
        .version = TRIDENT_SETTINGS_VER,
        .size = sizeof(TridentSettings),
        .settings = *settings,
    };

    if(storage_file_open(file, TRIDENT_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &blob, sizeof(blob));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
