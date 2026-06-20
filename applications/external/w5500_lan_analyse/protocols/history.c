#include "history.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define TAG "HISTORY"

void history_make_filename(const char* type, char* out, uint16_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    snprintf(
        out,
        out_size,
        "%04d%02d%02d_%02d%02d%02d_%s.txt",
        (int)dt.year,
        (int)dt.month,
        (int)dt.day,
        (int)dt.hour,
        (int)dt.minute,
        (int)dt.second,
        type);
}

bool history_save(const char* type, const char* content) {
    char filename[HISTORY_FILENAME_LEN];
    history_make_filename(type, filename, sizeof(filename));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, HISTORY_DIR);

    File* file = storage_file_alloc(storage);
    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char filepath[128];
    snprintf(filepath, sizeof(filepath), HISTORY_DIR "/%s", filename);

    bool ok = false;
    if(storage_file_open(file, filepath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, content, strlen(content)) == (uint16_t)strlen(content);
        storage_file_close(file);
        FURI_LOG_I(TAG, "History saved: %s", filepath);
    } else {
        FURI_LOG_E(TAG, "Failed to save: %s", filepath);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

uint16_t history_list(HistoryState* state) {
    memset(state, 0, sizeof(HistoryState));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    /* Build dir path, strip trailing slash if present (FatFS compat) */
    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char dir_path[128];
    snprintf(dir_path, sizeof(dir_path), "%s", HISTORY_DIR);
    size_t plen = strlen(dir_path);
    if(plen > 1 && dir_path[plen - 1] == '/') dir_path[plen - 1] = '\0';

    if(!storage_dir_open(dir, dir_path)) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    FileInfo info;
    char name[HISTORY_FILENAME_LEN];

    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(info.flags & FSF_DIRECTORY) continue;

        /* Only list .txt files */
        uint16_t nlen = strlen(name);
        if(nlen < 5) continue;
        if(strcmp(&name[nlen - 4], ".txt") != 0) continue;

        if(state->file_count < HISTORY_MAX_FILES) {
            HistoryEntry* e = &state->files[state->file_count];
            strncpy(e->filename, name, HISTORY_FILENAME_LEN - 1);

            /* Build short label: "MM-DD HH:MM type" from YYYYMMDD_HHMMSS_type.txt */
            if(nlen > 16 && name[8] == '_' && name[15] == '_') {
                const char* type_start = &name[16];
                uint16_t type_len = strlen(type_start);
                if(type_len > 4) type_len -= 4;
                if(type_len > 8) type_len = 8;
                snprintf(
                    e->label,
                    sizeof(e->label),
                    "%.2s-%.2s %.2s:%.2s %.*s",
                    name + 4,
                    name + 6,
                    name + 9,
                    name + 11,
                    type_len,
                    type_start);
            } else {
                strncpy(e->label, name, sizeof(e->label) - 1);
            }

            state->file_count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    return state->file_count;
}

bool history_read_file(const char* filename, char* out, uint16_t out_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char filepath[128];
    snprintf(filepath, sizeof(filepath), HISTORY_DIR "/%s", filename);

    bool ok = false;
    if(storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t read = storage_file_read(file, out, out_size - 1);
        out[read] = '\0';
        ok = read > 0;
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool history_delete_file(const char* filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char filepath[128];
    snprintf(filepath, sizeof(filepath), HISTORY_DIR "/%s", filename);

    bool ok = storage_simply_remove(storage, filepath);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Deleted: %s (%s)", filepath, ok ? "ok" : "fail");
    return ok;
}
