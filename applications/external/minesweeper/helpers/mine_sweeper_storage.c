#include <flipper_format/flipper_format.h>

#include "minesweeper.h"
#include "helpers/mine_sweeper_config.h"
#include "helpers/mine_sweeper_storage.h"

static inline uint32_t clamp(uint32_t min, uint32_t max, uint32_t val) {
    return (val > max) ? max : (val < min) ? min : val;
}

static Storage* mine_sweeper_open_storage(void) {
    return furi_record_open(RECORD_STORAGE);
}

static void mine_sweeper_close_storage(void) {
    furi_record_close(RECORD_STORAGE);
}

static void mine_sweeper_free_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_free(file);
}

static bool mine_sweeper_ensure_config_directory(Storage* storage) {
    FS_Error dir_stat = storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL);
    if(dir_stat == FSE_OK) return true;

    if(dir_stat == FSE_NOT_EXIST) {
        FURI_LOG_I(TAG, "Config dir missing, creating: %s", CONFIG_FILE_DIRECTORY_PATH);
        if(storage_common_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH) == FSE_OK) return true;

        FURI_LOG_E(TAG, "Config dir create failed: %s", CONFIG_FILE_DIRECTORY_PATH);
        return false;
    }

    FURI_LOG_E(TAG, "Config dir stat failed: %s (err=%d)", CONFIG_FILE_DIRECTORY_PATH, dir_stat);
    return false;
}

static bool mine_sweeper_read_uint32_or_default(
    FlipperFormat* file,
    const char* key,
    uint32_t* output,
    uint32_t fallback) {
    uint32_t value = fallback;

    if(!flipper_format_rewind(file) || !flipper_format_read_uint32(file, key, &value, 1)) {
        *output = fallback;
        return false;
    }

    *output = value;
    return true;
}

static bool mine_sweeper_write_settings_payload(FlipperFormat* file, const MineSweeperApp* app) {
    uint32_t w = app->settings_committed.board_width;
    uint32_t h = app->settings_committed.board_height;
    uint32_t d = app->settings_committed.difficulty;
    uint32_t f = app->feedback_enabled;
    uint32_t wr = app->wrap_enabled;
    uint32_t s = app->settings_committed.ensure_solvable_board ? 1U : 0U;

    if(!flipper_format_write_header_cstr(
           file, MINESWEEPER_SETTINGS_HEADER, MINESWEEPER_SETTINGS_FILE_VERSION)) {
        return false;
    }
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_WIDTH, &w, 1)) return false;
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_HEIGHT, &h, 1)) return false;
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_DIFFICULTY, &d, 1))
        return false;
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_FEEDBACK, &f, 1)) return false;
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_WRAP, &wr, 1)) return false;
    if(!flipper_format_write_uint32(file, MINESWEEPER_SETTINGS_KEY_SOLVABLE, &s, 1)) return false;

    return true;
}

static void mine_sweeper_try_cleanup_tmp(Storage* storage) {
    if(storage == NULL) return;

    if(storage_common_stat(storage, MINESWEEPER_SETTINGS_SAVE_PATH_TMP, NULL) == FSE_OK) {
        if(storage_common_remove(storage, MINESWEEPER_SETTINGS_SAVE_PATH_TMP) != FSE_OK) {
            FURI_LOG_W(
                TAG, "Failed to remove temp config: %s", MINESWEEPER_SETTINGS_SAVE_PATH_TMP);
        }
    }
}

void mine_sweeper_save_settings(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    Storage* storage = NULL;
    FlipperFormat* fff_file = NULL;
    bool save_ok = false;
    bool temp_file_open = false;

    storage = mine_sweeper_open_storage();
    if(storage == NULL) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return;
    }

    if(!mine_sweeper_ensure_config_directory(storage)) {
        goto cleanup;
    }

    mine_sweeper_try_cleanup_tmp(storage);

    fff_file = flipper_format_file_alloc(storage);
    if(fff_file == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate config file formatter");
        goto cleanup;
    }

    if(!flipper_format_file_open_new(fff_file, MINESWEEPER_SETTINGS_SAVE_PATH_TMP)) {
        FURI_LOG_E(
            TAG, "Failed to open temp config for write: %s", MINESWEEPER_SETTINGS_SAVE_PATH_TMP);
        goto cleanup;
    }
    temp_file_open = true;

    if(!mine_sweeper_write_settings_payload(fff_file, app)) {
        FURI_LOG_E(TAG, "Failed to serialize settings payload");
        goto cleanup;
    }

    if(!flipper_format_rewind(fff_file)) {
        FURI_LOG_E(TAG, "Failed to rewind temp config after write");
        goto cleanup;
    }

    if(!flipper_format_file_close(fff_file)) {
        FURI_LOG_E(TAG, "Failed to close temp config before rename");
        goto cleanup;
    }
    temp_file_open = false;

    FS_Error rename_err = storage_common_rename(
        storage, MINESWEEPER_SETTINGS_SAVE_PATH_TMP, MINESWEEPER_SETTINGS_SAVE_PATH);
    if(rename_err != FSE_OK) {
        FURI_LOG_E(
            TAG,
            "Atomic settings replace failed (err=%d): %s -> %s",
            rename_err,
            MINESWEEPER_SETTINGS_SAVE_PATH_TMP,
            MINESWEEPER_SETTINGS_SAVE_PATH);
        goto cleanup;
    }

    save_ok = true;

cleanup:
    if(temp_file_open) {
        flipper_format_file_close(fff_file);
    }
    mine_sweeper_free_config_file(fff_file);

    if(!save_ok) {
        mine_sweeper_try_cleanup_tmp(storage);
    }

    mine_sweeper_close_storage();
}

bool mine_sweeper_read_settings(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    Storage* storage = NULL;
    FlipperFormat* fff_file = NULL;
    FuriString* temp_str = NULL;
    bool read_ok = false;
    bool migrate_after_read = false;
    bool file_open = false;
    uint32_t file_version = 0;

    storage = mine_sweeper_open_storage();
    if(storage == NULL) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    if(storage_common_stat(storage, MINESWEEPER_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        goto cleanup;
    }

    fff_file = flipper_format_file_alloc(storage);
    if(fff_file == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate config file formatter");
        goto cleanup;
    }

    temp_str = furi_string_alloc();
    if(temp_str == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate temp string for config header");
        goto cleanup;
    }

    if(!flipper_format_file_open_existing(fff_file, MINESWEEPER_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open config file: %s", MINESWEEPER_SETTINGS_SAVE_PATH);
        goto cleanup;
    }
    file_open = true;

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Config header read failed");
        goto cleanup;
    }

    if(file_version > MINESWEEPER_SETTINGS_FILE_VERSION) {
        FURI_LOG_W(
            TAG,
            "Config version %lu is newer than supported %u",
            (unsigned long)file_version,
            MINESWEEPER_SETTINGS_FILE_VERSION);
        goto cleanup;
    }

    if(file_version < MINESWEEPER_SETTINGS_FILE_VERSION_MIN_SUPPORTED) {
        FURI_LOG_W(
            TAG,
            "Config version %lu below minimum supported %u",
            (unsigned long)file_version,
            MINESWEEPER_SETTINGS_FILE_VERSION_MIN_SUPPORTED);
        goto cleanup;
    }

    uint32_t w = 16;
    uint32_t h = 7;
    uint32_t d = 0;
    uint32_t f = 1;
    uint32_t wr = 1;
    uint32_t s = 0;

    if(!mine_sweeper_read_uint32_or_default(fff_file, MINESWEEPER_SETTINGS_KEY_WIDTH, &w, 16)) {
        FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_WIDTH);
        migrate_after_read = true;
    }
    if(!mine_sweeper_read_uint32_or_default(fff_file, MINESWEEPER_SETTINGS_KEY_HEIGHT, &h, 7)) {
        FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_HEIGHT);
        migrate_after_read = true;
    }
    if(!mine_sweeper_read_uint32_or_default(fff_file, MINESWEEPER_SETTINGS_KEY_DIFFICULTY, &d, 0)) {
        FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_DIFFICULTY);
        migrate_after_read = true;
    }
    if(!mine_sweeper_read_uint32_or_default(fff_file, MINESWEEPER_SETTINGS_KEY_FEEDBACK, &f, 1)) {
        FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_FEEDBACK);
        migrate_after_read = true;
    }
    if(!mine_sweeper_read_uint32_or_default(fff_file, MINESWEEPER_SETTINGS_KEY_WRAP, &wr, 1)) {
        FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_WRAP);
        migrate_after_read = true;
    }

    if(file_version >= MINESWEEPER_SETTINGS_FILE_VERSION) {
        if(!mine_sweeper_read_uint32_or_default(
               fff_file, MINESWEEPER_SETTINGS_KEY_SOLVABLE, &s, 0)) {
            FURI_LOG_W(TAG, "Missing/corrupt key: %s", MINESWEEPER_SETTINGS_KEY_SOLVABLE);
            migrate_after_read = true;
        }
    } else {
        s = 0;
        migrate_after_read = true;
    }

    w = clamp(16, 32, w);
    h = clamp(7, 32, h);
    d = clamp(0, 2, d);
    f = clamp(0, 1, f);
    wr = clamp(0, 1, wr);
    s = clamp(0, 1, s);

    app->settings_committed.board_width = (uint8_t)w;
    app->settings_committed.board_height = (uint8_t)h;
    app->settings_committed.difficulty = (uint8_t)d;
    app->settings_committed.ensure_solvable_board = (s != 0);
    app->feedback_enabled = (uint8_t)f;
    app->wrap_enabled = (uint8_t)wr;

    read_ok = true;

cleanup:
    if(file_open) {
        flipper_format_file_close(fff_file);
    }
    mine_sweeper_free_config_file(fff_file);
    if(temp_str) furi_string_free(temp_str);
    mine_sweeper_close_storage();

    if(read_ok && (migrate_after_read || file_version < MINESWEEPER_SETTINGS_FILE_VERSION)) {
        FURI_LOG_I(
            TAG,
            "Migrating config from v%lu to v%u",
            (unsigned long)file_version,
            MINESWEEPER_SETTINGS_FILE_VERSION);
        mine_sweeper_save_settings(app);
    }

    return read_ok;
}
