#include "call_history.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

const char* const call_method_names[CallMethodCount] = {
    "GET",
    "POST",
    "PUT",
    "DELETE",
    "PATCH",
    "HEAD",
};

const char* const call_protocol_names[CallProtocolCount] = {
    "HTTP",
    "HTTPS",
};

/** Build the history file path and make sure the app data dir exists. */
static void call_history_get_path(Storage* storage, FuriString* path) {
    furi_string_set_str(path, APP_DATA_PATH("call_history.txt"));
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
}

/** Persist app->call_history to storage. */
static bool call_history_save(AppContext* app) {
    furi_assert(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    bool ok = false;

    call_history_get_path(storage, path);

    do {
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_write_header_cstr(file, CALL_HISTORY_FILETYPE, CALL_HISTORY_VERSION)) {
            break;
        }
        for(uint8_t i = 0; i < app->call_history_count; i++) {
            if(!flipper_format_write_string_cstr(file, "URL", app->call_history[i].url)) {
                break;
            }
            if(!flipper_format_write_string_cstr(
                   file, "Protocol", call_protocol_names[app->call_history[i].protocol])) {
                break;
            }
            if(!flipper_format_write_string_cstr(
                   file, "Method", call_method_names[app->call_history[i].method])) {
                break;
            }
            if(!flipper_format_write_string_cstr(file, "Query", app->call_history[i].query)) {
                break;
            }
            if(!flipper_format_write_string_cstr(file, "Headers", app->call_history[i].headers)) {
                break;
            }
            if(!flipper_format_write_string_cstr(file, "Body", app->call_history[i].body)) {
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

static CallProtocol call_protocol_from_cstr(const char* value) {
    furi_assert(value);
    return strcmp(value, "HTTPS") == 0 ? CallProtocolHttps : CallProtocolHttp;
}

static CallMethod call_method_from_cstr(const char* value) {
    furi_assert(value);
    for(uint8_t i = 0; i < CallMethodCount; i++) {
        if(strcmp(value, call_method_names[i]) == 0) {
            return (CallMethod)i;
        }
    }
    return CallMethodGet;
}

void call_history_load(AppContext* app) {
    furi_assert(app);

    app->call_history_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    FuriString* value = furi_string_alloc();
    uint32_t version = 0;
    uint32_t count = 0;

    call_history_get_path(storage, path);

    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_read_header(file, value, &version)) {
            break;
        }
        if(!flipper_format_get_value_count(file, "URL", &count)) {
            break;
        }
        if(count > CALL_HISTORY_MAX) {
            count = CALL_HISTORY_MAX;
        }
        for(uint32_t i = 0; i < count; i++) {
            CallEntry* entry = &app->call_history[i];

            if(!flipper_format_read_string(file, "URL", value)) {
                break;
            }
            snprintf(entry->url, sizeof(entry->url), "%s", furi_string_get_cstr(value));

            if(!flipper_format_read_string(file, "Protocol", value)) {
                break;
            }
            entry->protocol = call_protocol_from_cstr(furi_string_get_cstr(value));

            if(!flipper_format_read_string(file, "Method", value)) {
                break;
            }
            entry->method = call_method_from_cstr(furi_string_get_cstr(value));

            if(!flipper_format_read_string(file, "Query", value)) {
                break;
            }
            snprintf(entry->query, sizeof(entry->query), "%s", furi_string_get_cstr(value));

            if(!flipper_format_read_string(file, "Headers", value)) {
                break;
            }
            snprintf(entry->headers, sizeof(entry->headers), "%s", furi_string_get_cstr(value));

            if(!flipper_format_read_string(file, "Body", value)) {
                break;
            }
            snprintf(entry->body, sizeof(entry->body), "%s", furi_string_get_cstr(value));

            app->call_history_count++;
        }
    } while(false);

    furi_string_free(value);
    furi_string_free(path);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool call_history_add(AppContext* app) {
    furi_assert(app);

    if(app->call_history_count >= CALL_HISTORY_MAX) {
        FURI_LOG_W("ApiCaller", "Call history is full");
        return false;
    }

    app->call_history[app->call_history_count] = app->call_form;
    app->call_history_count++;

    return call_history_save(app);
}

bool call_history_update(AppContext* app, uint8_t index) {
    furi_assert(app);

    if(index >= app->call_history_count) {
        return false;
    }

    app->call_history[index] = app->call_form;

    return call_history_save(app);
}

bool call_history_remove(AppContext* app, uint8_t index) {
    furi_assert(app);

    if(index >= app->call_history_count) {
        return false;
    }

    for(uint8_t i = index; i < app->call_history_count - 1; i++) {
        app->call_history[i] = app->call_history[i + 1];
    }
    app->call_history_count--;

    return call_history_save(app);
}
