#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <storage/storage.h>

#define POCKET_D20_PATH_LEN 96U
#define POCKET_D20_LONG_PATH_LEN 128U

/* Build a child path without relying on snprintf truncation. Prefix may be NULL. */
static inline bool dnd_fs_child_path(
    char* output,
    size_t size,
    const char* directory,
    const char* prefix,
    const char* name) {
    if(!output || size == 0U || !directory || !directory[0] || !name || !name[0]) return false;
    size_t directory_length = strlen(directory);
    size_t prefix_length = prefix ? strlen(prefix) : 0U;
    size_t name_length = strlen(name);
    if(directory_length > SIZE_MAX - prefix_length - name_length - 2U) return false;
    size_t required = directory_length + 1U + prefix_length + name_length + 1U;
    if(required > size) return false;

    memcpy(output, directory, directory_length);
    output[directory_length] = '/';
    if(prefix_length) memcpy(output + directory_length + 1U, prefix, prefix_length);
    memcpy(output + directory_length + 1U + prefix_length, name, name_length);
    output[required - 1U] = '\0';
    return true;
}

static inline bool dnd_fs_directory_exists(Storage* storage, const char* path) {
    if(!storage || !path || !path[0]) return false;
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

static inline bool dnd_fs_ensure_directory(Storage* storage, const char* path) {
    if(!storage || !path || path[0] != '/') return false;
    if(dnd_fs_directory_exists(storage, path)) return true;
    storage_common_mkdir(storage, path);
    return dnd_fs_directory_exists(storage, path);
}

/* Create every parent component before opening a writable file. */
static inline bool dnd_fs_ensure_parent_dir(Storage* storage, const char* path) {
    if(!storage || !path || path[0] != '/') return false;
    size_t length = strlen(path);
    if(length < 2U || length >= POCKET_D20_LONG_PATH_LEN) return false;

    char directory[POCKET_D20_LONG_PATH_LEN];
    memcpy(directory, path, length + 1U);
    char* last = strrchr(directory, '/');
    if(!last || last == directory) return true;
    *last = '\0';

    for(char* cursor = directory + 1U; *cursor; ++cursor) {
        if(*cursor != '/') continue;
        *cursor = '\0';
        if(!dnd_fs_ensure_directory(storage, directory)) return false;
        *cursor = '/';
    }
    return dnd_fs_ensure_directory(storage, directory);
}
