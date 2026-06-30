#include "settings_storage.h"
#include <furi.h> // for logging
#include "uart_storage.h"
static Storage* storage = NULL;

// Forward declarations of static functions
static bool write_header(File* file);
static bool verify_header(File* file);

bool settings_storage_init() {
    uint32_t start_time = furi_get_tick();
    FURI_LOG_I("SettingsStorage", "Starting storage initialization");

    if(storage != NULL) {
        FURI_LOG_I("SettingsStorage", "Storage already initialized");
        return true;
    }

    storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) {
        FURI_LOG_E("SettingsStorage", "Failed to open RECORD_STORAGE");
        return false;
    }

    // Attempt to create directory without checking if it exists
    if(!storage_simply_mkdir(storage, GHOST_ESP_APP_FOLDER)) {
        FURI_LOG_E("SettingsStorage", "Failed to create app directory");
        furi_record_close(RECORD_STORAGE);
        storage = NULL;
        return false;
    }

    uint32_t duration = furi_get_tick() - start_time;
    FURI_LOG_I(
        "SettingsStorage", "Storage initialization complete (Time taken: %lu ms)", duration);

    return true;
}

void uart_storage_sync_file(UartStorageContext* ctx) {
    if(ctx && ctx->current_file) {
        storage_file_sync(ctx->current_file);
    }
}

static bool write_header(File* file) {
    SettingsHeader header = {
        .magic = SETTINGS_HEADER_MAGIC,
        .version = SETTINGS_FILE_VERSION,
        .settings_count = SETTINGS_COUNT};
    return storage_file_write(file, &header, sizeof(header)) == sizeof(header);
}

static bool verify_header(File* file) {
    SettingsHeader header;
    if(storage_file_read(file, &header, sizeof(header)) != sizeof(header)) {
        return false;
    }
    return header.magic == SETTINGS_HEADER_MAGIC && header.version == SETTINGS_FILE_VERSION &&
           header.settings_count == SETTINGS_COUNT;
}

SettingsResult settings_storage_save(Settings* settings, const char* path) {
    FURI_LOG_I("SettingsStorage", "Starting to save settings to %s", path);

    if(!storage) {
        FURI_LOG_E("SettingsStorage", "Storage not initialized");
        return SETTINGS_FILE_ERROR;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E("SettingsStorage", "Failed to open file for writing: %s", path);
        storage_file_free(file);
        return SETTINGS_FILE_ERROR;
    }

    bool success = write_header(file);
    if(!success) {
        FURI_LOG_E("SettingsStorage", "Failed to write header");
    } else {
        size_t bytes_written = storage_file_write(file, settings, sizeof(Settings));
        if(bytes_written != sizeof(Settings)) {
            FURI_LOG_E("SettingsStorage", "Failed to write settings data");
            success = false;
        } else {
            FURI_LOG_I("SettingsStorage", "Settings saved successfully");
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return success ? SETTINGS_OK : SETTINGS_FILE_ERROR;
}

SettingsResult settings_storage_load(Settings* settings, const char* path) {
    FURI_LOG_D("SettingsStorage", "Loading settings from %s", path);

    if(!storage) {
        FURI_LOG_E("SettingsStorage", "Storage not initialized");
        return SETTINGS_FILE_ERROR;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_W("SettingsStorage", "Settings file doesn't exist, creating with defaults");
        storage_file_free(file);
        memset(settings, 0, sizeof(Settings));
        return settings_storage_save(settings, path);
    }

    bool success = verify_header(file);
    if(!success) {
        FURI_LOG_E("SettingsStorage", "Invalid header in settings file");
        storage_file_close(file);
        storage_file_free(file);
        return SETTINGS_PARSE_ERROR;
    }

    success = storage_file_read(file, settings, sizeof(Settings)) == sizeof(Settings);
    if(!success) {
        FURI_LOG_E("SettingsStorage", "Failed to read settings data");
    }

    storage_file_close(file);
    storage_file_free(file);
    return success ? SETTINGS_OK : SETTINGS_PARSE_ERROR;
}
