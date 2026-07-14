#include <furi.h>
#include "game_storage.h"

static Storage* game_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void game_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

static void game_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

void game_save_settings(void* context) {
    GameApp* app = context;
    if(app->save_settings == 0) {
        return;
    }

    FURI_LOG_D(TAG, "Saving Settings");
    Storage* storage = game_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    if(storage_file_exists(storage, GAME_SETTINGS_SAVE_PATH)) {
        storage_simply_remove(storage, GAME_SETTINGS_SAVE_PATH);
    }

    if(!storage_common_stat(storage, GAME_SETTINGS_SAVE_PATH, NULL) == FSE_OK) {
        FURI_LOG_D(TAG, "Config file %s is not found. Will create new.", GAME_SETTINGS_SAVE_PATH);
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            FURI_LOG_D(
                TAG, "Directory %s doesn't exist. Will create new.", CONFIG_FILE_DIRECTORY_PATH);
            if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
                FURI_LOG_E(TAG, "Error creating directory %s", CONFIG_FILE_DIRECTORY_PATH);
            }
        }
    }

    if(!flipper_format_file_open_new(fff_file, GAME_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Error creating new file %s", GAME_SETTINGS_SAVE_PATH);
        game_close_storage();
        return;
    }

    flipper_format_write_header_cstr(fff_file, GAME_SETTINGS_HEADER, GAME_SETTINGS_FILE_VERSION);
    flipper_format_write_uint32(fff_file, GAME_SETTINGS_KEY_HAPTIC, &app->haptic, 1);
    flipper_format_write_uint32(fff_file, GAME_SETTINGS_KEY_SPEAKER, &app->speaker, 1);
    flipper_format_write_uint32(fff_file, GAME_SETTINGS_KEY_LED, &app->led, 1);
    flipper_format_write_uint32(fff_file, GAME_SETTINGS_KEY_SAVE_SETTINGS, &app->save_settings, 1);

    if(!flipper_format_rewind(fff_file)) {
        game_close_config_file(fff_file);
        FURI_LOG_E(TAG, "Rewind error");
        game_close_storage();
        return;
    }

    game_close_config_file(fff_file);
    game_close_storage();
}

void game_read_settings(void* context) {
    GameApp* app = context;
    Storage* storage = game_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    if(storage_common_stat(storage, GAME_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        game_close_config_file(fff_file);
        game_close_storage();
        return;
    }
    uint32_t file_version;
    FuriString* temp_str = furi_string_alloc();

    if(!flipper_format_file_open_existing(fff_file, GAME_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open file %s", GAME_SETTINGS_SAVE_PATH);
        furi_string_free(temp_str);
        game_close_config_file(fff_file);
        game_close_storage();
        return;
    }

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Missing Header Data");
        furi_string_free(temp_str);
        game_close_config_file(fff_file);
        game_close_storage();
        return;
    }

    furi_string_free(temp_str);

    if(file_version < GAME_SETTINGS_FILE_VERSION) {
        FURI_LOG_I(TAG, "old config version, will be removed.");
        game_close_config_file(fff_file);
        game_close_storage();
        return;
    }

    flipper_format_read_uint32(fff_file, GAME_SETTINGS_KEY_HAPTIC, &app->haptic, 1);
    flipper_format_read_uint32(fff_file, GAME_SETTINGS_KEY_SPEAKER, &app->speaker, 1);
    flipper_format_read_uint32(fff_file, GAME_SETTINGS_KEY_LED, &app->led, 1);
    flipper_format_read_uint32(fff_file, GAME_SETTINGS_KEY_SAVE_SETTINGS, &app->save_settings, 1);

    flipper_format_rewind(fff_file);

    game_close_config_file(fff_file);
    game_close_storage();
}

void game_save_best_score(int best_score) {
    FURI_LOG_D(TAG, "Saving Best Score: %d", best_score);
    Storage* storage = game_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    if(storage_file_exists(storage, GAME_SCORE_SAVE_PATH)) {
        storage_simply_remove(storage, GAME_SCORE_SAVE_PATH);
    }

    if(!storage_common_stat(storage, GAME_SCORE_SAVE_PATH, NULL) == FSE_OK) {
        FURI_LOG_D(TAG, "Score file %s is not found. Will create new.", GAME_SCORE_SAVE_PATH);
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            FURI_LOG_D(
                TAG, "Directory %s doesn't exist. Will create new.", CONFIG_FILE_DIRECTORY_PATH);
            if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
                FURI_LOG_E(TAG, "Error creating directory %s", CONFIG_FILE_DIRECTORY_PATH);
            }
        }
    }

    if(!flipper_format_file_open_new(fff_file, GAME_SCORE_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Error creating new file %s", GAME_SCORE_SAVE_PATH);
        game_close_config_file(fff_file);
        game_close_storage();
        return;
    }

    uint32_t score_value = (uint32_t)best_score;
    flipper_format_write_header_cstr(fff_file, GAME_SCORE_HEADER, GAME_SCORE_FILE_VERSION);
    flipper_format_write_uint32(fff_file, GAME_SCORE_KEY_BEST, &score_value, 1);

    if(!flipper_format_rewind(fff_file)) {
        game_close_config_file(fff_file);
        FURI_LOG_E(TAG, "Rewind error");
        game_close_storage();
        return;
    }

    game_close_config_file(fff_file);
    game_close_storage();
}

int game_read_best_score(void) {
    Storage* storage = game_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);
    int best_score = 0;

    if(storage_common_stat(storage, GAME_SCORE_SAVE_PATH, NULL) != FSE_OK) {
        game_close_config_file(fff_file);
        game_close_storage();
        return 0;
    }

    uint32_t file_version;
    FuriString* temp_str = furi_string_alloc();

    if(!flipper_format_file_open_existing(fff_file, GAME_SCORE_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open file %s", GAME_SCORE_SAVE_PATH);
        furi_string_free(temp_str);
        game_close_config_file(fff_file);
        game_close_storage();
        return 0;
    }

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Missing Header Data");
        furi_string_free(temp_str);
        game_close_config_file(fff_file);
        game_close_storage();
        return 0;
    }

    furi_string_free(temp_str);

    if(file_version < GAME_SCORE_FILE_VERSION) {
        FURI_LOG_I(TAG, "old score file version, will be removed.");
        game_close_config_file(fff_file);
        game_close_storage();
        return 0;
    }

    uint32_t score_value = 0;
    if(flipper_format_read_uint32(fff_file, GAME_SCORE_KEY_BEST, &score_value, 1)) {
        best_score = (int)score_value;
    }

    game_close_config_file(fff_file);
    game_close_storage();

    return best_score;
}

void game_clear_best_score(void) {
    FURI_LOG_D(TAG, "Clearing Best Score");
    Storage* storage = game_open_storage();

    if(storage_file_exists(storage, GAME_SCORE_SAVE_PATH)) {
        storage_simply_remove(storage, GAME_SCORE_SAVE_PATH);
    }

    game_close_storage();
}
