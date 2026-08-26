#include "wifi_history.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

// Empty values do not round-trip through FlipperFormat reliably,
// so an empty password (open network) is stored as "-".
#define WIFI_EMPTY_PLACEHOLDER "-"

/** Placeholder for an empty password on write. */
static const char* wifi_history_store_str(const char* value) {
    return (value == NULL || value[0] == '\0') ? WIFI_EMPTY_PLACEHOLDER : value;
}

/** Read the password, mapping the placeholder back to empty. */
static void wifi_history_load_str(FuriString* value, char* out, size_t out_size) {
    if(strcmp(furi_string_get_cstr(value), WIFI_EMPTY_PLACEHOLDER) == 0) {
        out[0] = '\0';
    } else {
        snprintf(out, out_size, "%s", furi_string_get_cstr(value));
    }
}

/** Build the history file path and make sure the app data dir exists. */
static void wifi_history_get_path(Storage* storage, FuriString* path) {
    furi_string_set_str(path, APP_DATA_PATH("wifi_history.txt"));
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
}

/** Persist app->wifi_history to storage. */
static bool wifi_history_save(AppContext* app) {
    furi_assert(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    bool ok = false;

    wifi_history_get_path(storage, path);

    do {
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_write_header_cstr(file, WIFI_HISTORY_FILETYPE, WIFI_HISTORY_VERSION)) {
            break;
        }
        for(uint8_t i = 0; i < app->wifi_history_count; i++) {
            if(!flipper_format_write_string_cstr(file, "SSID", app->wifi_history[i].ssid)) {
                break;
            }
            if(!flipper_format_write_string_cstr(
                   file, "Password", wifi_history_store_str(app->wifi_history[i].password))) {
                break;
            }
        }
        ok = true;
    } while(false);

    flipper_format_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void wifi_history_load(AppContext* app) {
    furi_assert(app);

    app->wifi_history_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    FuriString* ssid = furi_string_alloc();
    FuriString* password = furi_string_alloc();
    uint32_t version = 0;
    uint32_t count = 0;

    wifi_history_get_path(storage, path);

    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_read_header(file, ssid, &version)) {
            break;
        }
        if(!flipper_format_get_value_count(file, "SSID", &count)) {
            break;
        }
        if(count > WIFI_HISTORY_MAX) {
            count = WIFI_HISTORY_MAX;
        }
        for(uint32_t i = 0; i < count; i++) {
            if(!flipper_format_read_string(file, "SSID", ssid)) {
                break;
            }
            if(!flipper_format_read_string(file, "Password", password)) {
                // Files written before the placeholder fix may lack the key
                // for empty passwords: fall back to an empty string.
                furi_string_set_str(password, "");
            }
            // Drop corrupted entries with an empty SSID (written by older buggy
            // app versions): they show up as nameless items and cannot connect.
            if(furi_string_empty(ssid)) {
                continue;
            }
            snprintf(
                app->wifi_history[i].ssid,
                sizeof(app->wifi_history[i].ssid),
                "%s",
                furi_string_get_cstr(ssid));
            wifi_history_load_str(
                password, app->wifi_history[i].password, sizeof(app->wifi_history[i].password));
            app->wifi_history_count++;
        }
    } while(false);

    furi_string_free(ssid);
    furi_string_free(password);
    furi_string_free(path);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool wifi_history_add(AppContext* app, const char* ssid, const char* password) {
    furi_assert(app);
    furi_assert(ssid);
    furi_assert(password);

    // Update the existing entry, if any
    for(uint8_t i = 0; i < app->wifi_history_count; i++) {
        if(strcmp(app->wifi_history[i].ssid, ssid) == 0) {
            snprintf(
                app->wifi_history[i].password,
                sizeof(app->wifi_history[i].password),
                "%s",
                password);
            return wifi_history_save(app);
        }
    }

    // New entry
    if(app->wifi_history_count >= WIFI_HISTORY_MAX) {
        FURI_LOG_W("ApiCaller", "WiFi history is full");
        return false;
    }

    snprintf(app->wifi_history[app->wifi_history_count].ssid, 64, "%s", ssid);
    snprintf(app->wifi_history[app->wifi_history_count].password, 64, "%s", password);
    app->wifi_history_count++;

    return wifi_history_save(app);
}
