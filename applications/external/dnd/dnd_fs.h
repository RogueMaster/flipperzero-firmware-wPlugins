#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <storage/storage.h>

#define DND_FS_PATH_LEN      96U
#define DND_FS_LONG_PATH_LEN 128U

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
    if(length < 2U || length >= DND_FS_LONG_PATH_LEN) return false;

    char directory[DND_FS_LONG_PATH_LEN];
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

/* All spellbook consumers must restore an interrupted sort before treating a
   missing live file as an empty collection. No full sorter is linked into them.
   Stat errors and directory collisions fail closed; renames never overwrite. */
static inline bool dnd_fs_recover_sort(Storage* storage, const char* live, bool* recovered) {
    if(recovered) *recovered = false;
    if(!storage || !live || !live[0]) return false;
    static const char suffix[] = ".sort.bak";
    size_t length = strlen(live);
    if(length + sizeof(suffix) > DND_FS_LONG_PATH_LEN) return false;
    char backup[DND_FS_LONG_PATH_LEN];
    memcpy(backup, live, length);
    memcpy(backup + length, suffix, sizeof(suffix));
    FileInfo info;
    FS_Error error = storage_common_stat(storage, backup, &info);
    if(error == FSE_NOT_EXIST) return true;
    if(error != FSE_OK || file_info_is_dir(&info)) return false;
    if(recovered) *recovered = true;
    error = storage_common_stat(storage, live, &info);
    if(error == FSE_NOT_EXIST)
        return storage_common_rename_safe(storage, backup, live) == FSE_OK;
    if(error != FSE_OK || file_info_is_dir(&info)) return false;
    return storage_common_remove(storage, backup) == FSE_OK;
}

/* Publish a synced temporary file, retaining the old live file on rename failure.
   If rollback itself fails, the backup remains available for recovery. */
static inline bool
    dnd_fs_publish(Storage* storage, const char* temp, const char* live, const char* backup) {
    bool had_live = storage_file_exists(storage, live);
    if(storage_file_exists(storage, backup)) {
        if(!had_live) {
            if(storage_common_rename(storage, backup, live) != FSE_OK) return false;
            had_live = true;
        } else if(storage_common_remove(storage, backup) != FSE_OK)
            return false;
    }
    if(had_live && storage_common_rename(storage, live, backup) != FSE_OK) return false;
    if(storage_common_rename(storage, temp, live) != FSE_OK) {
        if(had_live) (void)storage_common_rename(storage, backup, live);
        return false;
    }
    if(had_live) (void)storage_common_remove(storage, backup);
    return true;
}
