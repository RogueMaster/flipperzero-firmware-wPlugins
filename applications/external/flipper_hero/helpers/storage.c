
#include "storage.h"
#include <storage/storage.h>
// #include <flipper_format/flipper_format.h>

#define SAVE_DIR  STORAGE_APP_DATA_PATH_PREFIX
#define SAVE_FILE SAVE_DIR "/flipper_hero.save"
#define CONF_FILE SAVE_DIR "/flipper_hero_config.save"

static Storage* flipper_hero_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void flipper_hero_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

void save_game_config(Config* config) {
    Storage* storage = flipper_hero_open_storage();
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, GAME_CONFIG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, config, sizeof(Config));
    }

    storage_file_close(file);
    storage_file_free(file);

    flipper_hero_close_storage();
}

bool load_game_config(Config* config) {
    Storage* storage = flipper_hero_open_storage();
    storage_common_migrate(
        storage, EXT_PATH("apps/Games/game_flipper_hero_config.save"), CONF_FILE);

    File* file = storage_file_alloc(storage);
    uint16_t bytes_read = 0;
    if(storage_file_open(file, CONF_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bytes_read = storage_file_read(file, config, sizeof(Config));
    }
    storage_file_close(file);
    storage_file_free(file);

    flipper_hero_close_storage();
    return bytes_read == sizeof(Config);
}

void save_game_records(Record* record) {
    Storage* storage = flipper_hero_open_storage();
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, GAME_RECORDS_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, record, sizeof(Record));
    }

    storage_file_close(file);
    storage_file_free(file);

    flipper_hero_close_storage();
}

bool load_game_records(Record* record) {
    Storage* storage = flipper_hero_open_storage();
    storage_common_migrate(storage, EXT_PATH("apps/Games/game_flipper_hero.save"), SAVE_FILE);

    File* file = storage_file_alloc(storage);
    uint16_t bytes_read = 0;
    if(storage_file_open(file, SAVE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bytes_read = storage_file_read(file, record, sizeof(Record));
    }
    storage_file_close(file);
    storage_file_free(file);

    flipper_hero_close_storage();
    return bytes_read == sizeof(Record);
}
