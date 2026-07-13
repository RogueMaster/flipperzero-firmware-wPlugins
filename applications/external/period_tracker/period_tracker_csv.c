#include "period_tracker_csv.h"
#include "period_tracker_string_utils.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

#define TAG "PeriodTrackerCSV"

// Escape CSV field (handle quotes and newlines)
void csv_escape_field(const char* input, char* output, size_t output_size) {
    if(!input || !output || output_size == 0) return;

    size_t out_idx = 0;
    bool needs_quotes = false;

    // Check if we need quotes
    for(size_t i = 0; input[i] != '\0'; i++) {
        if(input[i] == ',' || input[i] == '"' || input[i] == '\n' || input[i] == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if(needs_quotes && out_idx < output_size - 1) {
        output[out_idx++] = '"';
    }

    for(size_t i = 0; input[i] != '\0' && out_idx < output_size - 2; i++) {
        if(input[i] == '"') {
            output[out_idx++] = '"'; // Double the quote
            if(out_idx < output_size - 1) {
                output[out_idx++] = '"';
            }
        } else {
            output[out_idx++] = input[i];
        }
    }

    if(needs_quotes && out_idx < output_size - 1) {
        output[out_idx++] = '"';
    }

    output[out_idx] = '\0';
}

// Unescape CSV field
bool csv_unescape_field(const char* input, char* output, size_t output_size) {
    if(!input || !output || output_size == 0) return false;

    size_t out_idx = 0;
    size_t in_idx = 0;
    bool in_quotes = false;

    // Check if field starts with quote
    if(input[0] == '"') {
        in_quotes = true;
        in_idx = 1;
    }

    while(input[in_idx] != '\0' && out_idx < output_size - 1) {
        if(in_quotes && input[in_idx] == '"') {
            if(input[in_idx + 1] == '"') {
                // Escaped quote
                output[out_idx++] = '"';
                in_idx += 2;
            } else {
                // End of quoted field
                break;
            }
        } else if(!in_quotes && input[in_idx] == ',') {
            // End of unquoted field
            break;
        } else {
            output[out_idx++] = input[in_idx++];
        }
    }

    output[out_idx] = '\0';
    return true;
}

// Build profile file path
void build_profile_path(const char* name, char* path, size_t path_size) {
    snprintf(path, path_size, APP_DATA_PATH("%s.profile"), name);
}

// Build events file path
void build_events_path(const char* name, char* path, size_t path_size) {
    snprintf(path, path_size, APP_DATA_PATH("%s.csv"), name);
}

// Build archive profile path
void build_archive_profile_path(const char* name, char* path, size_t path_size) {
    snprintf(path, path_size, APP_DATA_PATH("%s.archive.profile"), name);
}

// Save profile to file
bool profile_save(Storage* storage, const GirlProfile* profile) {
    if(!profile || !girl_profile_is_valid(profile)) {
        FURI_LOG_E(TAG, "profile_save: invalid profile");
        return false;
    }

    // Ensure the data directory exists (create parent directories too)
    FURI_LOG_I(TAG, "profile_save: creating directories");
    FS_Error err1 = storage_common_mkdir(storage, "/ext/apps_data");
    FURI_LOG_I(TAG, "parent dir result: %d", err1);
    // Remove trailing slash - use explicit path instead of macro
    FS_Error err2 = storage_common_mkdir(storage, "/ext/apps_data/period_tracker");
    FURI_LOG_I(TAG, "app dir result: %d", err2);

    char path[128];
    build_profile_path(profile->name, path, sizeof(path));
    FURI_LOG_I(TAG, "profile_save: path=%s", path);

    File* file = storage_file_alloc(storage);
    bool success = false;

    FURI_LOG_I(TAG, "profile_save: attempting to open file");
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_I(TAG, "profile_save: file opened successfully");
        char line[MAX_NOTES_LENGTH * 2 + 16];
        char escaped[MAX_NOTES_LENGTH * 2 + 4]; // Worst case: every char doubled + quotes + null

        // Write profile as key=value pairs
        snprintf(line, sizeof(line), "name=%s\n", profile->name);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "cycle_length_days=%u\n", profile->cycle_length_days);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "period_length_days=%u\n", profile->period_length_days);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "uses_contraception=%d\n", profile->uses_contraception);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "has_pcos=%d\n", profile->has_pcos);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "is_menopausal=%d\n", profile->is_menopausal);
        storage_file_write(file, line, strlen(line));

        csv_escape_field(profile->notes, escaped, sizeof(escaped));
        snprintf(line, sizeof(line), "notes=%s\n", escaped);
        storage_file_write(file, line, strlen(line));

        storage_file_close(file);
        success = true;
        FURI_LOG_I(TAG, "Profile saved: %s", profile->name);

        // Update the profile index (workaround for directory listing limitation)
        // Check the raw index lines (not the filtered profile list) so archived
        // profiles that are re-created don't produce duplicate index entries
        bool name_exists = false;
        File* check_file = storage_file_alloc(storage);
        if(storage_file_open(
               check_file, APP_DATA_PATH("profiles.idx"), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FileLineReader reader;
            char index_line[MAX_NAME_LENGTH + 4];
            file_line_reader_init(&reader, check_file);
            while(file_line_reader_next(&reader, index_line, sizeof(index_line))) {
                if(strcmp(index_line, profile->name) == 0) {
                    name_exists = true;
                    FURI_LOG_I(TAG, "Profile already in index: %s", profile->name);
                    break;
                }
            }
            storage_file_close(check_file);
        }
        storage_file_free(check_file);

        // Only append if name doesn't exist
        if(!name_exists) {
            File* index_file = storage_file_alloc(storage);
            if(storage_file_open(
                   index_file, APP_DATA_PATH("profiles.idx"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
                storage_file_write(index_file, profile->name, strlen(profile->name));
                storage_file_write(index_file, "\n", 1);
                storage_file_close(index_file);
                FURI_LOG_I(TAG, "Profile added to index: %s", profile->name);
            } else {
                // Index file doesn't exist, create it
                if(storage_file_open(
                       index_file, APP_DATA_PATH("profiles.idx"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    storage_file_write(index_file, profile->name, strlen(profile->name));
                    storage_file_write(index_file, "\n", 1);
                    storage_file_close(index_file);
                    FURI_LOG_I(TAG, "Profile index created with: %s", profile->name);
                }
            }
            storage_file_free(index_file);
        }
    } else {
        FS_Error error = storage_file_get_error(file);
        FURI_LOG_E(TAG, "Failed to open file for profile: %s, error: %d", profile->name, error);
        FURI_LOG_E(TAG, "Attempted path: %s", path);
    }

    storage_file_free(file);
    return success;
}

// Load profile from file
bool profile_load(Storage* storage, const char* name, GirlProfile* profile) {
    if(!name || !profile) return false;

    char path[128];
    build_profile_path(name, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        girl_profile_init(profile);
        strncpy(profile->name, name, MAX_NAME_LENGTH - 1);

        FileLineReader reader;
        char line[MAX_NOTES_LENGTH * 2 + 16]; // Long enough for escaped notes line
        file_line_reader_init(&reader, file);

        while(file_line_reader_next(&reader, line, sizeof(line))) {
            char* equals = strchr(line, '=');
            if(equals != NULL) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;

                if(strcmp(key, "cycle_length_days") == 0) {
                    profile->cycle_length_days = (uint8_t)atoi(value);
                } else if(strcmp(key, "period_length_days") == 0) {
                    profile->period_length_days = (uint8_t)atoi(value);
                } else if(strcmp(key, "uses_contraception") == 0) {
                    profile->uses_contraception = atoi(value) != 0;
                } else if(strcmp(key, "has_pcos") == 0) {
                    profile->has_pcos = atoi(value) != 0;
                } else if(strcmp(key, "is_menopausal") == 0) {
                    profile->is_menopausal = atoi(value) != 0;
                } else if(strcmp(key, "notes") == 0) {
                    csv_unescape_field(value, profile->notes, MAX_NOTES_LENGTH);
                }
            }
        }

        storage_file_close(file);
        success = true;
        FURI_LOG_I(TAG, "Profile loaded: %s", name);
    }

    storage_file_free(file);
    return success;
}

// Check if profile exists
bool profile_exists(Storage* storage, const char* name) {
    char path[128];
    build_profile_path(name, path, sizeof(path));
    return storage_file_exists(storage, path);
}

// Delete profile and events
bool profile_delete(Storage* storage, const char* name) {
    char path[128];

    // Delete profile
    build_profile_path(name, path, sizeof(path));
    bool profile_deleted = storage_simply_remove(storage, path);

    // Delete events
    build_events_path(name, path, sizeof(path));
    bool events_deleted = storage_simply_remove(storage, path);

    FURI_LOG_I(
        TAG,
        "Profile deleted: %s (profile: %d, events: %d)",
        name,
        profile_deleted,
        events_deleted);

    return profile_deleted;
}

// Archive profile (rename with .archive suffix)
bool profile_archive(Storage* storage, const char* name) {
    char old_path[128], new_path[128];

    // Rename profile
    build_profile_path(name, old_path, sizeof(old_path));
    build_archive_profile_path(name, new_path, sizeof(new_path));
    bool profile_archived = (storage_common_rename(storage, old_path, new_path) == FSE_OK);

    // Rename events
    build_events_path(name, old_path, sizeof(old_path));
    snprintf(new_path, sizeof(new_path), APP_DATA_PATH("%s.archive.csv"), name);
    bool events_archived = (storage_common_rename(storage, old_path, new_path) == FSE_OK);

    FURI_LOG_I(
        TAG,
        "Profile archived: %s (profile: %d, events: %d)",
        name,
        profile_archived,
        events_archived);

    return profile_archived;
}

// List all profile names
bool profile_list_all(
    Storage* storage,
    char names[][MAX_NAME_LENGTH],
    uint8_t* count,
    uint8_t max_count) {
    *count = 0;

    FURI_LOG_I(TAG, "profile_list_all: starting");

    // Ensure the data directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    // Try to read from index file (workaround for FAP apps)
    File* index_file = storage_file_alloc(storage);
    if(storage_file_open(
           index_file, APP_DATA_PATH("profiles.idx"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "profile_list_all: reading from index file");

        FileLineReader reader;
        char line[MAX_NAME_LENGTH + 4];
        file_line_reader_init(&reader, index_file);

        while(file_line_reader_next(&reader, line, sizeof(line)) && *count < max_count) {
            // Skip empty lines
            if(line[0] != '\0') {
                // Check if profile file actually exists (not archived)
                if(profile_exists(storage, line)) {
                    strncpy(names[*count], line, MAX_NAME_LENGTH - 1);
                    names[*count][MAX_NAME_LENGTH - 1] = '\0';
                    FURI_LOG_I(TAG, "profile_list_all: found profile: %s", names[*count]);
                    (*count)++;
                } else {
                    FURI_LOG_I(
                        TAG, "profile_list_all: skipping archived/deleted profile: %s", line);
                }
            }
        }

        storage_file_close(index_file);
        storage_file_free(index_file);
        FURI_LOG_I(TAG, "profile_list_all: found %u profiles from index", *count);
        return true;
    }

    storage_file_free(index_file);
    FURI_LOG_I(TAG, "profile_list_all: index file not found (first run?), returning 0 profiles");
    // Return true with 0 count - this is a valid state (no profiles yet)
    return true;
}

// Parse one CSV line into an event. Returns true on success.
static bool event_parse_line(char* line, CycleEvent* event) {
    // Parse CSV: date,event_type,value
    char* fields[3];
    int field_count = string_split_csv(line, fields, 3);

    if(field_count < 2) return false;
    if(!parse_date_string(fields[0], &event->date)) return false;

    if(strcmp(fields[1], "period_start") == 0) {
        event->type = EventTypePeriodStart;
    } else if(strcmp(fields[1], "symptom") == 0) {
        event->type = EventTypeSymptom;
    } else {
        FURI_LOG_W(TAG, "Unknown event type: '%s'", fields[1]);
        return false;
    }

    if(field_count >= 3) {
        csv_unescape_field(fields[2], event->value, MAX_SYMPTOM_NAME_LENGTH);
    } else {
        event->value[0] = '\0'; // Empty value for period_start
    }
    return true;
}

// Load events for a girl, keeping the MOST RECENT max_events (by file order)
bool events_load_all(
    Storage* storage,
    const char* girl_name,
    CycleEvent* events,
    uint16_t* count,
    uint16_t max_events) {
    *count = 0;

    char path[128];
    build_events_path(girl_name, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    FileLineReader reader;
    char line[160];
    CycleEvent parsed;
    bool first_line = true;

    // Pass 1: count valid events so we know how many leading ones to skip
    uint32_t total_valid = 0;
    file_line_reader_init(&reader, file);
    while(file_line_reader_next(&reader, line, sizeof(line))) {
        if(first_line) {
            first_line = false; // Skip header
            continue;
        }
        if(event_parse_line(line, &parsed)) total_valid++;
    }

    uint32_t skip = (total_valid > max_events) ? (total_valid - max_events) : 0;

    // Pass 2: load the last max_events events
    storage_file_seek(file, 0, true);
    file_line_reader_init(&reader, file);
    first_line = true;
    uint32_t seen = 0;
    while(file_line_reader_next(&reader, line, sizeof(line)) && *count < max_events) {
        if(first_line) {
            first_line = false;
            continue;
        }
        if(event_parse_line(line, &parsed)) {
            if(seen++ < skip) continue;
            events[*count] = parsed;
            (*count)++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    FURI_LOG_I(TAG, "Loaded %u of %lu events for %s", *count, total_valid, girl_name);
    return true;
}

// Append event to events file
bool event_append(Storage* storage, const char* girl_name, const CycleEvent* event) {
    // Ensure the data directory exists
    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    char path[128];
    build_events_path(girl_name, path, sizeof(path));

    // Check if file exists to determine if we need header
    bool file_exists = storage_file_exists(storage, path);

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(
           file, path, FSAM_WRITE, file_exists ? FSOM_OPEN_APPEND : FSOM_CREATE_ALWAYS)) {
        // Write header if new file
        if(!file_exists) {
            const char* header = "date,event_type,value\n";
            storage_file_write(file, header, strlen(header));
        }

        // Write event
        char line[256];
        char date_str[16];
        char value_escaped[MAX_SYMPTOM_NAME_LENGTH * 2];
        const char* type_str = (event->type == EventTypePeriodStart) ? "period_start" : "symptom";

        format_date_string(&event->date, date_str, sizeof(date_str));
        csv_escape_field(event->value, value_escaped, sizeof(value_escaped));

        snprintf(line, sizeof(line), "%s,%s,%s\n", date_str, type_str, value_escaped);
        success = storage_file_write(file, line, strlen(line));

        storage_file_close(file);

        if(success) {
            FURI_LOG_I(TAG, "Event appended for %s: %s", girl_name, type_str);
        }
    }

    storage_file_free(file);
    return success;
}

// Get last period start date (optimized - streams CSV without allocating 22KB)
bool events_get_last_period_start(Storage* storage, const char* girl_name, SimpleDate* date) {
    char path[128];
    build_events_path(girl_name, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    bool found = false;
    SimpleDate latest = {0};
    FileLineReader reader;
    char line[160];
    bool first_line = true;

    // Stream through CSV file line by line
    file_line_reader_init(&reader, file);
    while(file_line_reader_next(&reader, line, sizeof(line))) {
        // Skip header line
        if(first_line) {
            first_line = false;
            continue;
        }

        // Parse CSV: date,event_type,value
        char* fields[3];
        int field_count = string_split_csv(line, fields, 3);

        if(field_count >= 2) {
            // Only process period_start events
            if(strcmp(fields[1], "period_start") == 0) {
                SimpleDate event_date;
                if(parse_date_string(fields[0], &event_date)) {
                    // Keep track of the latest (most recent) period start
                    if(!found || compare_dates(&event_date, &latest) > 0) {
                        latest = event_date;
                        found = true;
                    }
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    if(found) {
        *date = latest;
    }

    return found;
}

// Save settings
bool settings_save(Storage* storage, const AppSettings* settings) {
    // Ensure the data directory exists
    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[256];

        snprintf(line, sizeof(line), "pin_enabled=%d\n", settings->pin_enabled ? 1 : 0);
        storage_file_write(file, line, strlen(line));

        snprintf(line, sizeof(line), "alert_lead_days=%u\n", settings->alert_lead_days);
        storage_file_write(file, line, strlen(line));

        // Write symptoms
        for(uint8_t i = 0; i < settings->symptom_count && i < MAX_SYMPTOMS; i++) {
            snprintf(line, sizeof(line), "symptom=%s\n", settings->global_symptoms[i]);
            storage_file_write(file, line, strlen(line));
        }

        storage_file_close(file);
        success = true;
        FURI_LOG_I(TAG, "Settings saved");
    }

    storage_file_free(file);
    return success;
}

// Load settings
bool settings_load(Storage* storage, AppSettings* settings) {
    FURI_LOG_I(TAG, "settings_load: starting");
    File* file = storage_file_alloc(storage);
    bool success = false;

    FURI_LOG_I(TAG, "settings_load: trying to open settings.txt");
    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "settings_load: file opened, reading");
        app_settings_init(settings);

        FileLineReader reader;
        char line[MAX_SYMPTOM_NAME_LENGTH + 32];
        file_line_reader_init(&reader, file);

        while(file_line_reader_next(&reader, line, sizeof(line))) {
            char* equals = strchr(line, '=');
            if(equals != NULL) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;

                if(strcmp(key, "alarm_time") == 0) {
                    // Ignore deprecated alarm_time setting - FAP apps cannot set RTC alarms
                } else if(strcmp(key, "pin_enabled") == 0) {
                    settings->pin_enabled = (atoi(value) != 0);
                } else if(strcmp(key, "alert_lead_days") == 0) {
                    settings->alert_lead_days = (uint8_t)atoi(value);
                    if(settings->alert_lead_days == 0)
                        settings->alert_lead_days = 1; // Minimum 1 day
                } else if(strcmp(key, "symptom") == 0 && settings->symptom_count < MAX_SYMPTOMS) {
                    strncpy(
                        settings->global_symptoms[settings->symptom_count],
                        value,
                        MAX_SYMPTOM_NAME_LENGTH - 1);
                    settings->symptom_count++;
                }
            }
        }

        storage_file_close(file);
        success = true;

        // A settings file without symptom entries would leave the symptom
        // picker empty - fall back to defaults so logging keeps working
        if(settings->symptom_count == 0) {
            app_settings_add_default_symptoms(settings);
        }

        FURI_LOG_I(TAG, "Settings loaded from file");
    } else {
        // No settings file, use defaults (don't try to save yet)
        FURI_LOG_I(TAG, "settings_load: file not found, using defaults");
        app_settings_init(settings);
        app_settings_add_default_symptoms(settings);
        // Don't call settings_save here - it might hang. Let it be created on first actual use.
        success = true; // Return success with defaults
    }

    storage_file_free(file);
    FURI_LOG_I(TAG, "settings_load: done, success=%d", success);
    return success;
}
