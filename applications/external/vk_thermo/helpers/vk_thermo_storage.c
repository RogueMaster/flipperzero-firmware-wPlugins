#include "vk_thermo_storage.h"
#include "../vk_thermo.h"
#include <datetime/datetime.h>
#include <stdio.h>

static Storage* vk_thermo_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void vk_thermo_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

static void vk_thermo_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

// Temperature conversion
float vk_thermo_celsius_to_fahrenheit(float celsius) {
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

float vk_thermo_celsius_to_kelvin(float celsius) {
    return celsius + 273.15f;
}

// UID to hex string (full)
void vk_thermo_uid_to_string(const uint8_t* uid, char* str, size_t str_len) {
    if(str_len < (VK_THERMO_UID_LENGTH * 2 + 1)) return;
    for(size_t i = 0; i < VK_THERMO_UID_LENGTH; i++) {
        snprintf(str + (i * 2), 3, "%02X", uid[i]);
    }
}

// UID to short string (last 4 chars)
void vk_thermo_uid_to_short_string(const uint8_t* uid, char* str, size_t str_len) {
    if(str_len < 7) return; // "..XXXX\0"
    snprintf(
        str, str_len, "..%02X%02X", uid[VK_THERMO_UID_LENGTH - 2], uid[VK_THERMO_UID_LENGTH - 1]);
}

// Settings functions
void vk_thermo_save_settings(void* context) {
    VkThermo* app = context;

    FURI_LOG_D(TAG, "Saving Settings");
    Storage* storage = vk_thermo_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    // Overwrite wont work, so delete first
    if(storage_file_exists(storage, VK_THERMO_SETTINGS_SAVE_PATH)) {
        storage_simply_remove(storage, VK_THERMO_SETTINGS_SAVE_PATH);
    }

    // Create directory if needed
    if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
        FURI_LOG_D(
            TAG, "Directory %s doesn't exist. Will create new.", CONFIG_FILE_DIRECTORY_PATH);
        if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
            FURI_LOG_E(TAG, "Error creating directory %s", CONFIG_FILE_DIRECTORY_PATH);
        }
    }

    if(!flipper_format_file_open_new(fff_file, VK_THERMO_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Error creating new file %s", VK_THERMO_SETTINGS_SAVE_PATH);
        vk_thermo_close_config_file(fff_file);
        vk_thermo_close_storage();
        return;
    }

    // Store Settings
    flipper_format_write_header_cstr(
        fff_file, VK_THERMO_SETTINGS_HEADER, VK_THERMO_SETTINGS_FILE_VERSION);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_HAPTIC, &app->haptic, 1);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_SPEAKER, &app->speaker, 1);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_LED, &app->led, 1);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_TEMP_UNIT, &app->temp_unit, 1);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_EH_TIMEOUT, &app->eh_timeout, 1);
    flipper_format_write_uint32(fff_file, VK_THERMO_SETTINGS_KEY_DEBUG, &app->debug, 1);

    vk_thermo_close_config_file(fff_file);
    vk_thermo_close_storage();
}

void vk_thermo_read_settings(void* context) {
    VkThermo* app = context;
    Storage* storage = vk_thermo_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    if(storage_common_stat(storage, VK_THERMO_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        vk_thermo_close_config_file(fff_file);
        vk_thermo_close_storage();
        return;
    }

    uint32_t file_version;
    FuriString* temp_str = furi_string_alloc();

    if(!flipper_format_file_open_existing(fff_file, VK_THERMO_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open file %s", VK_THERMO_SETTINGS_SAVE_PATH);
        vk_thermo_close_config_file(fff_file);
        vk_thermo_close_storage();
        furi_string_free(temp_str);
        return;
    }

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Missing Header Data");
        vk_thermo_close_config_file(fff_file);
        vk_thermo_close_storage();
        furi_string_free(temp_str);
        return;
    }

    furi_string_free(temp_str);

    if(file_version < VK_THERMO_SETTINGS_FILE_VERSION) {
        FURI_LOG_I(TAG, "old config version, will be removed.");
        vk_thermo_close_config_file(fff_file);
        vk_thermo_close_storage();
        return;
    }

    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_HAPTIC, &app->haptic, 1);
    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_SPEAKER, &app->speaker, 1);
    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_LED, &app->led, 1);
    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_TEMP_UNIT, &app->temp_unit, 1);
    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_EH_TIMEOUT, &app->eh_timeout, 1);
    flipper_format_read_uint32(fff_file, VK_THERMO_SETTINGS_KEY_DEBUG, &app->debug, 1);

    vk_thermo_close_config_file(fff_file);
    vk_thermo_close_storage();
}

// Log functions
void vk_thermo_log_init(VkThermoLog* log) {
    memset(log, 0, sizeof(VkThermoLog));
    log->count = 0;
    log->head = 0;
}

void vk_thermo_log_add_entry(VkThermoLog* log, const uint8_t* uid, float temperature) {
    VkThermoLogEntry* entry = &log->entries[log->head];

    memcpy(entry->uid, uid, VK_THERMO_UID_LENGTH);
    entry->timestamp = furi_hal_rtc_get_timestamp();
    entry->temperature_celsius = temperature;
    entry->valid = true;

    log->head = (log->head + 1) % VK_THERMO_LOG_MAX_ENTRIES;
    if(log->count < VK_THERMO_LOG_MAX_ENTRIES) {
        log->count++;
    }

    // Auto-save to CSV
    vk_thermo_log_save_csv(log);
}

void vk_thermo_log_clear(VkThermoLog* log) {
    vk_thermo_log_init(log);
    vk_thermo_log_delete_csv();
}

// Build CSV file path for a given UID: /ext/apps_data/vk_thermo/<UID_HEX>.csv
static void vk_thermo_build_uid_csv_path(const uint8_t* uid, char* path, size_t path_len) {
    char uid_str[VK_THERMO_UID_LENGTH * 2 + 1];
    vk_thermo_uid_to_string(uid, uid_str, sizeof(uid_str));
    snprintf(path, path_len, "%s/%s.csv", CONFIG_FILE_DIRECTORY_PATH, uid_str);
}

// Save entries for a single UID to its CSV file
static void vk_thermo_log_save_uid_csv(Storage* storage, VkThermoLog* log, const uint8_t* uid) {
    char path[128];
    vk_thermo_build_uid_csv_path(uid, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open CSV for writing: %s", path);
        storage_file_free(file);
        return;
    }

    // No uid column - the filename IS the uid
    const char* header = "timestamp,celsius,fahrenheit\n";
    storage_file_write(file, header, strlen(header));

    char line[96];
    uint8_t written = 0;

    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx;
        if(log->count < VK_THERMO_LOG_MAX_ENTRIES) {
            idx = i;
        } else {
            idx = (log->head + i) % VK_THERMO_LOG_MAX_ENTRIES;
        }

        VkThermoLogEntry* entry = &log->entries[idx];
        if(!entry->valid) continue;
        if(memcmp(entry->uid, uid, VK_THERMO_UID_LENGTH) != 0) continue;

        float fahrenheit = vk_thermo_celsius_to_fahrenheit(entry->temperature_celsius);
        int len = snprintf(
            line,
            sizeof(line),
            "%lu,%.2f,%.2f\n",
            (unsigned long)entry->timestamp,
            (double)entry->temperature_celsius,
            (double)fahrenheit);

        storage_file_write(file, line, len);
        written++;
    }

    storage_file_close(file);
    storage_file_free(file);
    FURI_LOG_D(TAG, "Saved %d entries to %s", written, path);
}

void vk_thermo_log_save_csv(VkThermoLog* log) {
    Storage* storage = vk_thermo_open_storage();

    // Create directory if needed
    if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
        storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH);
    }

    // Collect unique UIDs from log
    uint8_t unique_uids[10][VK_THERMO_UID_LENGTH];
    uint8_t uid_count = 0;

    for(uint8_t i = 0; i < log->count && uid_count < 10; i++) {
        uint8_t idx;
        if(log->count < VK_THERMO_LOG_MAX_ENTRIES) {
            idx = i;
        } else {
            idx = (log->head + i) % VK_THERMO_LOG_MAX_ENTRIES;
        }

        VkThermoLogEntry* entry = &log->entries[idx];
        if(!entry->valid) continue;

        bool found = false;
        for(uint8_t j = 0; j < uid_count; j++) {
            if(memcmp(unique_uids[j], entry->uid, VK_THERMO_UID_LENGTH) == 0) {
                found = true;
                break;
            }
        }
        if(!found) {
            memcpy(unique_uids[uid_count], entry->uid, VK_THERMO_UID_LENGTH);
            uid_count++;
        }
    }

    // Save one CSV file per UID
    for(uint8_t i = 0; i < uid_count; i++) {
        vk_thermo_log_save_uid_csv(storage, log, unique_uids[i]);
    }

    // Remove legacy single-file CSV if it exists
    if(storage_file_exists(storage, VK_THERMO_CSV_LEGACY_PATH)) {
        storage_simply_remove(storage, VK_THERMO_CSV_LEGACY_PATH);
        FURI_LOG_I(TAG, "Removed legacy readings.csv");
    }

    vk_thermo_close_storage();
}

// Parse UID from a hex filename (e.g., "E0040150A1B2C3D4" from "E0040150A1B2C3D4.csv")
static bool vk_thermo_parse_uid_from_filename(const char* name, uint8_t* uid) {
    // Filename must be exactly 16 hex chars + ".csv" = 20 chars
    size_t len = strlen(name);
    if(len != VK_THERMO_UID_LENGTH * 2 + 4) return false;
    if(strcmp(name + VK_THERMO_UID_LENGTH * 2, ".csv") != 0) return false;

    for(size_t i = 0; i < VK_THERMO_UID_LENGTH; i++) {
        char byte_str[3] = {name[i * 2], name[i * 2 + 1], '\0'};
        char* end;
        long val = strtol(byte_str, &end, 16);
        if(*end != '\0') return false;
        uid[i] = (uint8_t)val;
    }
    return true;
}

// Load entries from a single UID CSV file
static void vk_thermo_log_load_uid_csv(
    Storage* storage,
    VkThermoLog* log,
    const uint8_t* uid,
    const char* path) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }

    char line[96];
    bool first_line = true;
    FuriString* line_str = furi_string_alloc();

    while(true) {
        furi_string_reset(line_str);
        char c;
        while(storage_file_read(file, &c, 1) == 1) {
            if(c == '\n') break;
            if(c != '\r') {
                furi_string_push_back(line_str, c);
            }
        }

        if(furi_string_size(line_str) == 0) break;

        if(first_line) {
            first_line = false;
            continue;
        }

        // Parse: timestamp,celsius,fahrenheit
        const char* line_cstr = furi_string_get_cstr(line_str);
        strncpy(line, line_cstr, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';

        char* timestamp_str = line;
        char* comma1 = strchr(timestamp_str, ',');
        if(!comma1) continue;
        *comma1 = '\0';

        char* celsius_str = comma1 + 1;
        char* comma2 = strchr(celsius_str, ',');
        if(comma2) *comma2 = '\0';

        if(timestamp_str[0] && celsius_str[0]) {
            VkThermoLogEntry* entry = &log->entries[log->head];
            memcpy(entry->uid, uid, VK_THERMO_UID_LENGTH);
            entry->timestamp = (uint32_t)strtoul(timestamp_str, NULL, 10);
            entry->temperature_celsius = strtof(celsius_str, NULL);
            entry->valid = true;

            log->head = (log->head + 1) % VK_THERMO_LOG_MAX_ENTRIES;
            if(log->count < VK_THERMO_LOG_MAX_ENTRIES) {
                log->count++;
            }
        }
    }

    furi_string_free(line_str);
    storage_file_close(file);
    storage_file_free(file);
}

// Load legacy readings.csv (old format with uid column) and migrate
static bool vk_thermo_log_load_legacy_csv(Storage* storage, VkThermoLog* log) {
    if(!storage_file_exists(storage, VK_THERMO_CSV_LEGACY_PATH)) return false;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, VK_THERMO_CSV_LEGACY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    char line[128];
    bool first_line = true;
    FuriString* line_str = furi_string_alloc();

    while(true) {
        furi_string_reset(line_str);
        char c;
        while(storage_file_read(file, &c, 1) == 1) {
            if(c == '\n') break;
            if(c != '\r') {
                furi_string_push_back(line_str, c);
            }
        }

        if(furi_string_size(line_str) == 0) break;

        if(first_line) {
            first_line = false;
            continue;
        }

        // Parse legacy format: uid,timestamp,celsius,fahrenheit
        const char* line_cstr = furi_string_get_cstr(line_str);
        strncpy(line, line_cstr, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';

        char* uid_str = line;
        char* comma1 = strchr(uid_str, ',');
        if(!comma1) continue;
        *comma1 = '\0';

        char* timestamp_str = comma1 + 1;
        char* comma2 = strchr(timestamp_str, ',');
        if(!comma2) continue;
        *comma2 = '\0';

        char* celsius_str = comma2 + 1;
        char* comma3 = strchr(celsius_str, ',');
        if(comma3) *comma3 = '\0';

        if(uid_str[0] && timestamp_str[0] && celsius_str[0]) {
            VkThermoLogEntry* entry = &log->entries[log->head];

            for(size_t i = 0; i < VK_THERMO_UID_LENGTH && uid_str[i * 2] && uid_str[i * 2 + 1];
                i++) {
                char byte_str[3] = {uid_str[i * 2], uid_str[i * 2 + 1], '\0'};
                entry->uid[i] = (uint8_t)strtol(byte_str, NULL, 16);
            }

            entry->timestamp = (uint32_t)strtoul(timestamp_str, NULL, 10);
            entry->temperature_celsius = strtof(celsius_str, NULL);
            entry->valid = true;

            log->head = (log->head + 1) % VK_THERMO_LOG_MAX_ENTRIES;
            if(log->count < VK_THERMO_LOG_MAX_ENTRIES) {
                log->count++;
            }
        }
    }

    furi_string_free(line_str);
    storage_file_close(file);
    storage_file_free(file);

    FURI_LOG_I(TAG, "Migrated %d entries from legacy readings.csv", log->count);
    return true;
}

void vk_thermo_log_load_csv(VkThermoLog* log) {
    vk_thermo_log_init(log);

    Storage* storage = vk_thermo_open_storage();

    // Try to load legacy format first (will be re-saved as per-UID files)
    bool migrated = vk_thermo_log_load_legacy_csv(storage, log);

    // Scan directory for per-UID CSV files (<UID>.csv)
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, CONFIG_FILE_DIRECTORY_PATH)) {
        char name[64];
        FileInfo fileinfo;

        while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
            uint8_t uid[VK_THERMO_UID_LENGTH];
            if(!vk_thermo_parse_uid_from_filename(name, uid)) continue;

            char path[128];
            snprintf(path, sizeof(path), "%s/%s", CONFIG_FILE_DIRECTORY_PATH, name);
            vk_thermo_log_load_uid_csv(storage, log, uid, path);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    vk_thermo_close_storage();

    // If we migrated legacy data, re-save in new per-UID format
    if(migrated && log->count > 0) {
        vk_thermo_log_save_csv(log);
    }

    FURI_LOG_D(TAG, "Loaded %d entries from CSV files", log->count);
}

void vk_thermo_log_delete_csv(void) {
    Storage* storage = vk_thermo_open_storage();

    // Delete all CSV files in the directory
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, CONFIG_FILE_DIRECTORY_PATH)) {
        char name[64];
        FileInfo fileinfo;

        while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
            // Delete any .csv file (both UID files and legacy)
            size_t len = strlen(name);
            if(len > 4 && strcmp(name + len - 4, ".csv") == 0) {
                char path[128];
                snprintf(path, sizeof(path), "%s/%s", CONFIG_FILE_DIRECTORY_PATH, name);
                storage_simply_remove(storage, path);
                FURI_LOG_D(TAG, "Deleted %s", name);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    vk_thermo_close_storage();
}

// Stats functions
float vk_thermo_log_get_min(VkThermoLog* log) {
    if(log->count == 0) return 0.0f;

    float min = 999.0f;
    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx = (log->count < VK_THERMO_LOG_MAX_ENTRIES) ?
                          i :
                          ((log->head + i) % VK_THERMO_LOG_MAX_ENTRIES);
        if(log->entries[idx].valid && log->entries[idx].temperature_celsius < min) {
            min = log->entries[idx].temperature_celsius;
        }
    }
    return min;
}

float vk_thermo_log_get_max(VkThermoLog* log) {
    if(log->count == 0) return 0.0f;

    float max = -999.0f;
    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx = (log->count < VK_THERMO_LOG_MAX_ENTRIES) ?
                          i :
                          ((log->head + i) % VK_THERMO_LOG_MAX_ENTRIES);
        if(log->entries[idx].valid && log->entries[idx].temperature_celsius > max) {
            max = log->entries[idx].temperature_celsius;
        }
    }
    return max;
}

float vk_thermo_log_get_avg(VkThermoLog* log) {
    if(log->count == 0) return 0.0f;

    float sum = 0.0f;
    uint8_t valid_count = 0;
    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx = (log->count < VK_THERMO_LOG_MAX_ENTRIES) ?
                          i :
                          ((log->head + i) % VK_THERMO_LOG_MAX_ENTRIES);
        if(log->entries[idx].valid) {
            sum += log->entries[idx].temperature_celsius;
            valid_count++;
        }
    }
    return valid_count > 0 ? sum / valid_count : 0.0f;
}

float vk_thermo_log_get_min_for_uid(VkThermoLog* log, const uint8_t* uid) {
    if(log->count == 0) return 0.0f;

    float min = 999.0f;
    bool found = false;
    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx = (log->count < VK_THERMO_LOG_MAX_ENTRIES) ?
                          i :
                          ((log->head + i) % VK_THERMO_LOG_MAX_ENTRIES);
        if(log->entries[idx].valid &&
           memcmp(log->entries[idx].uid, uid, VK_THERMO_UID_LENGTH) == 0) {
            if(log->entries[idx].temperature_celsius < min) {
                min = log->entries[idx].temperature_celsius;
            }
            found = true;
        }
    }
    return found ? min : 0.0f;
}

float vk_thermo_log_get_max_for_uid(VkThermoLog* log, const uint8_t* uid) {
    if(log->count == 0) return 0.0f;

    float max = -999.0f;
    bool found = false;
    for(uint8_t i = 0; i < log->count; i++) {
        uint8_t idx = (log->count < VK_THERMO_LOG_MAX_ENTRIES) ?
                          i :
                          ((log->head + i) % VK_THERMO_LOG_MAX_ENTRIES);
        if(log->entries[idx].valid &&
           memcmp(log->entries[idx].uid, uid, VK_THERMO_UID_LENGTH) == 0) {
            if(log->entries[idx].temperature_celsius > max) {
                max = log->entries[idx].temperature_celsius;
            }
            found = true;
        }
    }
    return found ? max : 0.0f;
}
