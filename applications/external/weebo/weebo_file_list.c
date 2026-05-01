#include "weebo_file_list.h"
#include <furi.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>

#define TAG "WeeboFileList"

struct WeeboFileList {
    FuriString** files;
    size_t count;
    size_t current_index;
};

WeeboFileList* weebo_file_list_alloc() {
    WeeboFileList* file_list = malloc(sizeof(WeeboFileList));
    file_list->files = NULL;
    file_list->count = 0;
    file_list->current_index = 0;
    return file_list;
}

void weebo_file_list_free(WeeboFileList* file_list) {
    furi_assert(file_list);
    weebo_file_list_reset(file_list);
    free(file_list);
}

void weebo_file_list_reset(WeeboFileList* file_list) {
    furi_assert(file_list);
    if(file_list->files) {
        for(size_t i = 0; i < file_list->count; i++) {
            furi_string_free(file_list->files[i]);
        }
        free(file_list->files);
        file_list->files = NULL;
    }
    file_list->count = 0;
    file_list->current_index = 0;
}

bool weebo_file_list_scan(WeeboFileList* file_list, Storage* storage, const char* directory) {
    furi_assert(file_list);
    furi_assert(storage);
    furi_assert(directory);

    weebo_file_list_reset(file_list);

    File* file = storage_file_alloc(storage);
    FileInfo file_info;

    if(!storage_dir_open(file, directory)) {
        FURI_LOG_E(TAG, "Failed to open directory: %s", directory);
        storage_file_free(file);
        return false;
    }

    // Count .nfc files first (up to limit)
    size_t file_count = 0;
    char path[256];
    while(storage_dir_read(file, &file_info, path, sizeof(path))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t len = strlen(path);
        if(len > 4 && strcmp(path + len - 4, NFC_APP_EXTENSION) == 0) {
            file_count++;
            if(file_count >= WEEBO_MAX_NFC_FILES) {
                FURI_LOG_W(
                    TAG, "Reached maximum file limit (%d), stopping scan", WEEBO_MAX_NFC_FILES);
                break;
            }
        }
    }

    if(file_count == 0) {
        FURI_LOG_D(TAG, "No .nfc files found in directory");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    // Allocate array for file paths
    file_list->files = malloc(sizeof(FuriString*) * file_count);
    file_list->count = 0;

    // Reset directory to read again
    storage_file_close(file);
    if(!storage_dir_open(file, directory)) {
        FURI_LOG_E(TAG, "Failed to reopen directory: %s", directory);
        storage_file_free(file);
        free(file_list->files);
        file_list->files = NULL;
        return false;
    }

    // Fill array with file paths (up to limit)
    while(storage_dir_read(file, &file_info, path, sizeof(path))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t len = strlen(path);
        if(len > 4 && strcmp(path + len - 4, NFC_APP_EXTENSION) == 0) {
            FuriString* full_path = furi_string_alloc();
            furi_string_printf(full_path, "%s/%s", directory, path);
            file_list->files[file_list->count] = full_path;
            file_list->count++;

            if(file_list->count >= WEEBO_MAX_NFC_FILES) {
                break;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    // Sort the files alphabetically for consistent order
    for(size_t i = 0; i < file_list->count - 1; i++) {
        for(size_t j = i + 1; j < file_list->count; j++) {
            if(furi_string_cmp(file_list->files[i], file_list->files[j]) > 0) {
                FuriString* temp = file_list->files[i];
                file_list->files[i] = file_list->files[j];
                file_list->files[j] = temp;
            }
        }
    }

    FURI_LOG_D(TAG, "Found %zu .nfc files", file_list->count);
    return true;
}

size_t weebo_file_list_get_count(WeeboFileList* file_list) {
    furi_assert(file_list);
    return file_list->count;
}

const char* weebo_file_list_get_current_path(WeeboFileList* file_list) {
    furi_assert(file_list);
    if(file_list->count == 0 || file_list->current_index >= file_list->count) {
        return NULL;
    }
    return furi_string_get_cstr(file_list->files[file_list->current_index]);
}

size_t weebo_file_list_get_current_index(WeeboFileList* file_list) {
    furi_assert(file_list);
    return file_list->current_index;
}

bool weebo_file_list_set_current_path(WeeboFileList* file_list, const char* path) {
    furi_assert(file_list);
    furi_assert(path);

    for(size_t i = 0; i < file_list->count; i++) {
        if(strcmp(furi_string_get_cstr(file_list->files[i]), path) == 0) {
            file_list->current_index = i;
            return true;
        }
    }
    return false;
}

bool weebo_file_list_cycle(WeeboFileList* file_list, WeeboCycleDirection direction) {
    furi_assert(file_list);
    if(file_list->count == 0) return false;

    if(direction == WeeboCycleDirectionNext) {
        file_list->current_index = (file_list->current_index + 1) % file_list->count;
    } else {
        if(file_list->current_index == 0) {
            file_list->current_index = file_list->count - 1;
        } else {
            file_list->current_index--;
        }
    }
    return true;
}
