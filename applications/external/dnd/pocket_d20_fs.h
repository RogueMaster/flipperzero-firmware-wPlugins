#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <storage/storage.h>

static inline bool pocket_d20_directory_exists(Storage* storage, const char* path) {
    if(!storage || !path || !path[0]) return false;
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

static inline bool pocket_d20_ensure_directory(Storage* storage, const char* path) {
    if(!storage || !path || path[0] != '/') return false;
    if(pocket_d20_directory_exists(storage, path)) return true;
    storage_common_mkdir(storage, path);
    return pocket_d20_directory_exists(storage, path);
}

/* Create every parent component before opening a writable file. */
static inline bool pocket_d20_ensure_parent_dir(Storage* storage, const char* path) {
    if(!storage || !path || path[0] != '/') return false;
    size_t length = strlen(path);
    if(length < 2U || length >= 256U) return false;

    char directory[256];
    memcpy(directory, path, length + 1U);
    char* last = strrchr(directory, '/');
    if(!last || last == directory) return true;
    *last = '\0';

    for(char* cursor = directory + 1U; *cursor; ++cursor) {
        if(*cursor != '/') continue;
        *cursor = '\0';
        if(!pocket_d20_ensure_directory(storage, directory)) return false;
        *cursor = '/';
    }
    return pocket_d20_ensure_directory(storage, directory);
}
