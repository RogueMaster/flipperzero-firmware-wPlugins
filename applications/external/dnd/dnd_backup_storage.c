#include "dnd_backup_storage.h"

#include "dnd_fs.h"
#include "dnd_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_BACKUP_DATA_DIR         "/ext/apps_data/dndolphins"
#define DND_BACKUP_MAX_BUNDLE_FILES 64U

bool dnd_backup_storage_valid_directory(const char* path) {
    if(!path) return false;
    size_t path_len = strlen(path);
    if(path_len >= DND_FS_LONG_PATH_LEN) return false;
    if(!strcmp(path, "/ext")) return true;
    if(strncmp(path, "/ext/", 5U) || !path[5]) return false;

    const char* segment = path + 5U;
    while(*segment) {
        const char* end = strchr(segment, '/');
        size_t length = end ? (size_t)(end - segment) : strlen(segment);
        if(!length || (length == 1U && segment[0] == '.') ||
           (length == 2U && segment[0] == '.' && segment[1] == '.'))
            return false;
        for(size_t i = 0U; i < length; ++i)
            if((uint8_t)segment[i] < 0x20U || segment[i] == '\\') return false;
        if(!end) break;
        segment = end + 1U;
        if(!*segment) return false;
    }
    return true;
}

static bool dnd_backup_ensure_directory(Storage* storage, const char* path) {
    if(!storage || !path || path[0] != '/') return false;
    if(dnd_fs_directory_exists(storage, path)) return true;
    size_t len = strlen(path);
    if(len < 2U || len >= DND_FS_LONG_PATH_LEN) return false;
    char build[DND_FS_LONG_PATH_LEN];
    memcpy(build, path, len + 1U);
    for(char* cursor = build + 1U; *cursor; ++cursor) {
        if(*cursor != '/') continue;
        *cursor = '\0';
        if(!dnd_fs_ensure_directory(storage, build)) return false;
        *cursor = '/';
    }
    return dnd_fs_ensure_directory(storage, build);
}

static bool dnd_backup_copy_file(Storage* storage, const char* source, const char* destination) {
    if(!storage || !source || !destination) return false;
    File* in = storage_file_alloc(storage);
    File* out = storage_file_alloc(storage);
    uint8_t* buffer = malloc(512U);
    if(!in || !out || !buffer) {
        if(in) storage_file_free(in);
        if(out) storage_file_free(out);
        free(buffer);
        return false;
    }
    bool ok = storage_file_open(in, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(out, destination, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    while(ok) {
        size_t read = storage_file_read(in, buffer, 512U);
        if(!read) break;
        ok = storage_file_write(out, buffer, read) == read;
    }
    if(ok) ok = storage_file_get_error(in) == FSE_OK;
    if(ok) ok = storage_file_sync(out);
    storage_file_close(in);
    storage_file_close(out);
    storage_file_free(in);
    storage_file_free(out);
    free(buffer);
    if(!ok) storage_common_remove(storage, destination);
    return ok;
}

static uint8_t dnd_backup_level(const DndCharacter* character) {
    if(!character) return 1U;
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        total += character->classes[i].level;
    if(total < 1U) total = 1U;
    if(total > 20U) total = 20U;
    return (uint8_t)total;
}

static bool dnd_backup_core_name(
    const char* filename,
    uint32_t profile,
    uint8_t* level,
    char* stem,
    size_t stem_size) {
    if(!filename) return false;
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "ch_%lu_", (unsigned long)profile);
    size_t prefix_len = strlen(prefix);
    size_t len = strlen(filename);
    if(strncmp(filename, prefix, prefix_len) != 0 || len < prefix_len + 6U ||
       strcmp(filename + len - 4U, ".shd") != 0)
        return false;
    const char* end = filename + len - 4U;
    const char* underscore = end;
    while(underscore > filename && underscore[-1] != '_')
        --underscore;
    if(underscore <= filename || underscore == end) return false;
    uint16_t parsed = 0U;
    for(const char* p = underscore; p < end; ++p) {
        if(*p < '0' || *p > '9') return false;
        parsed = (uint16_t)(parsed * 10U + (uint16_t)(*p - '0'));
        if(parsed > 20U) return false;
    }
    if(parsed < 1U) return false;
    if(level) *level = (uint8_t)parsed;
    if(stem && stem_size) {
        size_t stem_len = len - 4U;
        if(stem_len + 1U > stem_size) return false;
        memcpy(stem, filename, stem_len);
        stem[stem_len] = '\0';
    }
    return true;
}

static bool dnd_backup_bundle_name(const char* filename, const char* stem) {
    if(!filename || !stem) return false;
    size_t len = strlen(filename);
    size_t stem_len = strlen(stem);
    if(len < stem_len + 4U || strcmp(filename + len - 4U, ".shd") != 0) return false;
    if(strncmp(filename, stem, stem_len) != 0) return false;
    return filename[stem_len] == '.' || filename[stem_len] == '_';
}

uint8_t dnd_backup_storage_list_core_files(
    Storage* storage,
    uint32_t profile,
    const char* directory,
    char names[][DND_BACKUP_CORE_NAME_LEN],
    uint8_t capacity,
    uint16_t* total) {
    if(total) *total = 0U;
    if(!storage || !directory || !directory[0]) return 0U;
    File* dir = storage_file_alloc(storage);
    if(!dir || !storage_dir_open(dir, directory)) {
        if(dir) storage_file_free(dir);
        return 0U;
    }
    uint8_t count = 0U;
    uint16_t seen = 0U;
    FileInfo info;
    char filename[DND_BACKUP_CORE_NAME_LEN];
    while(storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) || !dnd_backup_core_name(filename, profile, NULL, NULL, 0U))
            continue;
        if(seen < UINT16_MAX) ++seen;
        if(names && count < capacity) {
            strncpy(names[count], filename, DND_BACKUP_CORE_NAME_LEN - 1U);
            names[count][DND_BACKUP_CORE_NAME_LEN - 1U] = '\0';
            ++count;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    if(total) *total = seen;
    return count;
}

bool dnd_backup_storage_export_bundle(
    Storage* storage,
    uint32_t profile,
    const char* destination_dir) {
    if(!storage || !dnd_backup_storage_valid_directory(destination_dir)) return false;
    DndSaveData* data = calloc(1U, sizeof(DndSaveData));
    if(!data) return false;
    bool recovered = false;
    bool ok = dnd_storage_load_profile(storage, profile, data, &recovered);
    if(ok) ok = dnd_storage_save_profile_updated(storage, profile, data);
    if(ok && !strcmp(destination_dir, DND_BACKUP_DATA_DIR)) {
        dnd_data_clear(data);
        free(data);
        return true;
    }
    if(ok) ok = dnd_backup_ensure_directory(storage, destination_dir);

    char stem[DND_BACKUP_CORE_NAME_LEN] = "";
    if(ok) {
        const uint8_t wanted_level = dnd_backup_level(&data->character);
        File* dir = storage_file_alloc(storage);
        ok = dir && storage_dir_open(dir, DND_BACKUP_DATA_DIR);
        if(ok) {
            FileInfo info;
            char filename[DND_BACKUP_CORE_NAME_LEN];
            while(storage_dir_read(dir, &info, filename, sizeof(filename))) {
                uint8_t level = 0U;
                if(!file_info_is_dir(&info) &&
                   dnd_backup_core_name(filename, profile, &level, stem, sizeof(stem)) &&
                   level == wanted_level)
                    break;
                stem[0] = '\0';
            }
            storage_dir_close(dir);
            ok = stem[0] != '\0';
        }
        if(dir) storage_file_free(dir);
    }

    char marker_name[DND_BACKUP_CORE_NAME_LEN] = "";
    char marker_source[DND_FS_LONG_PATH_LEN] = "";
    char marker_destination[DND_FS_LONG_PATH_LEN] = "";
    if(ok) {
        int marker_length = snprintf(marker_name, sizeof(marker_name), "%s_bundle.shd", stem);
        ok = marker_length > 0 && (size_t)marker_length < sizeof(marker_name) &&
             dnd_fs_child_path(
                 marker_source, sizeof(marker_source), DND_BACKUP_DATA_DIR, NULL, marker_name) &&
             dnd_fs_child_path(
                 marker_destination,
                 sizeof(marker_destination),
                 destination_dir,
                 NULL,
                 marker_name) &&
             storage_file_exists(storage, marker_source);
        if(ok && storage_file_exists(storage, marker_destination))
            ok = storage_common_remove(storage, marker_destination) == FSE_OK;
    }

    if(ok) {
        File* dir = storage_file_alloc(storage);
        ok = dir && storage_dir_open(dir, DND_BACKUP_DATA_DIR);
        if(ok) {
            FileInfo info;
            char filename[DND_BACKUP_CORE_NAME_LEN];
            uint8_t copied = 0U;
            while(ok && storage_dir_read(dir, &info, filename, sizeof(filename))) {
                if(file_info_is_dir(&info) || !dnd_backup_bundle_name(filename, stem) ||
                   !strcmp(filename, marker_name))
                    continue;
                if(copied == DND_BACKUP_MAX_BUNDLE_FILES - 1U) {
                    ok = false;
                    break;
                }
                char source[DND_FS_LONG_PATH_LEN], destination[DND_FS_LONG_PATH_LEN];
                if(!dnd_fs_child_path(
                       source, sizeof(source), DND_BACKUP_DATA_DIR, NULL, filename) ||
                   !dnd_fs_child_path(
                       destination, sizeof(destination), destination_dir, NULL, filename)) {
                    ok = false;
                    break;
                }
                ok = dnd_backup_copy_file(storage, source, destination);
                if(ok) ++copied;
            }
            storage_dir_close(dir);
        }
        if(dir) storage_file_free(dir);
    }
    /* The completeness marker is the bundle commit record. Publish it only
       after every core/sidecar file was copied successfully so an interrupted
       export cannot be mistaken for a complete backup. */
    if(ok) ok = dnd_backup_copy_file(storage, marker_source, marker_destination);
    dnd_data_clear(data);
    free(data);
    return ok;
}

static bool dnd_backup_collect_names(
    Storage* storage,
    const char* directory,
    const char* stem,
    char (**out_names)[DND_BACKUP_CORE_NAME_LEN],
    uint8_t* out_count) {
    if(out_names) *out_names = NULL;
    if(out_count) *out_count = 0U;
    File* dir = storage_file_alloc(storage);
    if(!dir || !storage_dir_open(dir, directory)) {
        if(dir) storage_file_free(dir);
        return false;
    }
    uint8_t count = 0U;
    FileInfo info;
    char filename[DND_BACKUP_CORE_NAME_LEN];
    while(storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if(!file_info_is_dir(&info) && dnd_backup_bundle_name(filename, stem)) {
            if(count == DND_BACKUP_MAX_BUNDLE_FILES) {
                storage_dir_close(dir);
                storage_file_free(dir);
                return false;
            }
            ++count;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    if(!count) return false;

    char(*names)[DND_BACKUP_CORE_NAME_LEN] = calloc(count, DND_BACKUP_CORE_NAME_LEN);
    if(!names) return false;
    dir = storage_file_alloc(storage);
    if(!dir || !storage_dir_open(dir, directory)) {
        if(dir) storage_file_free(dir);
        free(names);
        return false;
    }
    uint8_t used = 0U;
    while(used < count && storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info) || !dnd_backup_bundle_name(filename, stem)) continue;
        strncpy(names[used], filename, DND_BACKUP_CORE_NAME_LEN - 1U);
        names[used][DND_BACKUP_CORE_NAME_LEN - 1U] = '\0';
        ++used;
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    if(used != count) {
        free(names);
        return false;
    }
    *out_names = names;
    *out_count = count;
    return true;
}

bool dnd_backup_storage_restore_bundle(
    Storage* storage,
    uint32_t profile,
    const char* core_shd_path,
    DndSaveData* restored) {
    if(!storage || !core_shd_path || !restored ||
       !dnd_storage_validate_character_path(storage, core_shd_path))
        return false;

    const char* filename = strrchr(core_shd_path, '/');
    filename = filename ? filename + 1U : core_shd_path;
    char stem[DND_BACKUP_CORE_NAME_LEN];
    if(!dnd_backup_core_name(filename, profile, NULL, stem, sizeof(stem))) return false;
    const char* slash = strrchr(core_shd_path, '/');
    if(!slash) return false;
    char source_dir[DND_FS_LONG_PATH_LEN];
    size_t source_len = (size_t)(slash - core_shd_path);
    if(!source_len || source_len >= sizeof(source_dir)) return false;
    memcpy(source_dir, core_shd_path, source_len);
    source_dir[source_len] = '\0';

    char(*source_names)[DND_BACKUP_CORE_NAME_LEN] = NULL;
    uint8_t source_count = 0U;
    if(!dnd_backup_collect_names(storage, source_dir, stem, &source_names, &source_count))
        return false;

    /* Preserve every existing internal file for this exact bundle stem so an
       interrupted/invalid external import cannot damage SHD history. */
    char(*old_names)[DND_BACKUP_CORE_NAME_LEN] = NULL;
    uint8_t old_count = 0U;
    (void)dnd_backup_collect_names(storage, DND_BACKUP_DATA_DIR, stem, &old_names, &old_count);

    bool ok = true;
    for(uint8_t i = 0U; ok && i < old_count; ++i) {
        char current[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
        if(!dnd_fs_child_path(current, sizeof(current), DND_BACKUP_DATA_DIR, NULL, old_names[i])) {
            ok = false;
            break;
        }
        snprintf(
            backup,
            sizeof(backup),
            "%s/.dndbak_%u_%u.tmp",
            DND_BACKUP_DATA_DIR,
            (unsigned)profile,
            (unsigned)i);
        storage_common_remove(storage, backup);
        ok = dnd_backup_copy_file(storage, current, backup);
    }
    for(uint8_t i = 0U; ok && i < source_count; ++i) {
        char source[DND_FS_LONG_PATH_LEN], stage[DND_FS_LONG_PATH_LEN];
        if(!dnd_fs_child_path(source, sizeof(source), source_dir, NULL, source_names[i])) {
            ok = false;
            break;
        }
        snprintf(
            stage,
            sizeof(stage),
            "%s/.dndin_%u_%u.tmp",
            DND_BACKUP_DATA_DIR,
            (unsigned)profile,
            (unsigned)i);
        storage_common_remove(storage, stage);
        ok = dnd_backup_copy_file(storage, source, stage);
    }

    if(ok) {
        for(uint8_t i = 0U; i < old_count; ++i) {
            char current[DND_FS_LONG_PATH_LEN];
            if(!dnd_fs_child_path(
                   current, sizeof(current), DND_BACKUP_DATA_DIR, NULL, old_names[i])) {
                ok = false;
                break;
            }
            if(storage_file_exists(storage, current))
                ok = storage_common_remove(storage, current) == FSE_OK && ok;
        }
    }
    for(uint8_t i = 0U; ok && i < source_count; ++i) {
        char stage[DND_FS_LONG_PATH_LEN], destination[DND_FS_LONG_PATH_LEN];
        snprintf(
            stage,
            sizeof(stage),
            "%s/.dndin_%u_%u.tmp",
            DND_BACKUP_DATA_DIR,
            (unsigned)profile,
            (unsigned)i);
        if(!dnd_fs_child_path(
               destination, sizeof(destination), DND_BACKUP_DATA_DIR, NULL, source_names[i])) {
            ok = false;
            break;
        }
        ok = storage_common_rename(storage, stage, destination) == FSE_OK;
    }

    char imported_core[DND_FS_LONG_PATH_LEN];
    if(ok)
        ok = dnd_fs_child_path(
            imported_core, sizeof(imported_core), DND_BACKUP_DATA_DIR, NULL, filename);
    if(ok) ok = dnd_storage_restore_shd_path(storage, profile, imported_core, restored);

    if(!ok) {
        /* Remove imported files, then put the previous internal history back. */
        for(uint8_t i = 0U; i < source_count; ++i) {
            char destination[DND_FS_LONG_PATH_LEN], stage[DND_FS_LONG_PATH_LEN];
            if(!dnd_fs_child_path(
                   destination, sizeof(destination), DND_BACKUP_DATA_DIR, NULL, source_names[i])) {
                ok = false;
                break;
            }
            snprintf(
                stage,
                sizeof(stage),
                "%s/.dndin_%u_%u.tmp",
                DND_BACKUP_DATA_DIR,
                (unsigned)profile,
                (unsigned)i);
            if(storage_file_exists(storage, destination))
                storage_common_remove(storage, destination);
            storage_common_remove(storage, stage);
        }
        for(uint8_t i = 0U; i < old_count; ++i) {
            char current[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
            if(!dnd_fs_child_path(
                   current, sizeof(current), DND_BACKUP_DATA_DIR, NULL, old_names[i])) {
                ok = false;
                break;
            }
            snprintf(
                backup,
                sizeof(backup),
                "%s/.dndbak_%u_%u.tmp",
                DND_BACKUP_DATA_DIR,
                (unsigned)profile,
                (unsigned)i);
            if(storage_file_exists(storage, backup))
                (void)storage_common_rename(storage, backup, current);
        }
    }

    for(uint8_t i = 0U; i < old_count; ++i) {
        char backup[DND_FS_LONG_PATH_LEN];
        snprintf(
            backup,
            sizeof(backup),
            "%s/.dndbak_%u_%u.tmp",
            DND_BACKUP_DATA_DIR,
            (unsigned)profile,
            (unsigned)i);
        storage_common_remove(storage, backup);
    }
    for(uint8_t i = 0U; i < source_count; ++i) {
        char stage[DND_FS_LONG_PATH_LEN];
        snprintf(
            stage,
            sizeof(stage),
            "%s/.dndin_%u_%u.tmp",
            DND_BACKUP_DATA_DIR,
            (unsigned)profile,
            (unsigned)i);
        storage_common_remove(storage, stage);
    }
    free(old_names);
    free(source_names);
    return ok;
}
