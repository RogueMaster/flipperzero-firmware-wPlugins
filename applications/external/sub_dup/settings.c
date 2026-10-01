#include "settings.h"
#include "app_state.h"
#include "logic.h"
#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define SETTINGS_FILE APP_DATA_PATH("folder.txt")

static void strip_trailing_newline(char* buf) {
    size_t len = strlen(buf);
    while(len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
}

// A hand-edited file may carry a trailing slash; scan_dir_is_valid doesn't reject it, but it
// would make the basename (and so the menu label) empty.
static void strip_trailing_slash(char* buf) {
    size_t len = strlen(buf);
    if(len > 0 && buf[len - 1] == '/') {
        buf[len - 1] = '\0';
    }
}

void settings_load(char* out, size_t cap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    char buf[FULL_PATH_LEN] = {0};
    // storage_file_exists resolves with a plain stat, unlike storage_file_open, which creates
    // the app data folder as a side effect even when opened read-only.
    bool ok = storage_file_exists(storage, SETTINGS_FILE);

    if(ok) {
        File* file = storage_file_alloc(storage);
        ok = storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING);

        if(ok) {
            uint64_t size = storage_file_size(file);
            // A file this big was never written by settings_save; don't guess at its content.
            if(size == 0 || size >= sizeof(buf)) {
                ok = false;
            } else {
                size_t read = storage_file_read(file, buf, (size_t)size);
                ok = (read == size);
                buf[read] = '\0';
            }
        }
        storage_file_close(file);
        storage_file_free(file);
    }

    if(ok) {
        strip_trailing_newline(buf);
        strip_trailing_slash(buf);
        // f_stat("") is FR_INVALID_NAME for the SD root, so storage_dir_exists("/ext") always
        // fails; the root's existence is implied by the storage record being open at all.
        ok = scan_dir_is_valid(buf) && (path_is_ext_root(buf) || storage_dir_exists(storage, buf));
    }

    snprintf(out, cap, "%s", ok ? buf : DEFAULT_SCAN_DIR);
    furi_record_close(RECORD_STORAGE);
}

bool settings_save(const char* dir) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(ok) {
        size_t len = strlen(dir);
        ok = storage_file_write(file, dir, len) == len;
    }

    ok = storage_file_close(file) && ok;
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
