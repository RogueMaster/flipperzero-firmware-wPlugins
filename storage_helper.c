#include "storage_helper.h"
#include "logic.h"
#include <furi.h>
#include <stdint.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define READ_BUFFER_SIZE 256

static bool file_crc32(Storage *storage, const char *path, uint32_t *crc, uint32_t *size) {
    File *file = storage_file_alloc(storage);
    if (!file)
        return false;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);

    if (ok) {
        uint64_t file_size = storage_file_size(file);
        uint32_t hash = 0;

        if (file_size <= UINT32_MAX) {
            uint8_t buffer[READ_BUFFER_SIZE];
            uint64_t bytes_read = 0;
            size_t read;
            while ((read = storage_file_read(file, buffer, READ_BUFFER_SIZE)) > 0) {
                hash = calculate_crc32(hash, buffer, read);
                bytes_read += read;
            }
            ok = (bytes_read == file_size);
        } else {
            ok = false;
        }

        if (ok) {
            *crc = hash;
            *size = (uint32_t)file_size;
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return ok;
}

typedef struct {
    Storage *storage;
    const char *dir;
} FileCompareContext;

static bool file_name_valid(const char *name) {
    return name && name[0] && strlen(name) < APP_MAX_PATH_LEN && !strchr(name, '/') &&
           !strchr(name, '\\') && is_sub_file(name);
}

static bool files_equal(const FileRecord *a, const FileRecord *b, void *context) {
    FileCompareContext *compare = context;
    char path_a[FULL_PATH_LEN], path_b[FULL_PATH_LEN];
    if (!file_name_valid(a->path) || !file_name_valid(b->path) || !strcmp(a->path, b->path) ||
        a->size != b->size || a->hash != b->hash ||
        !path_join(path_a, sizeof(path_a), compare->dir, a->path) ||
        !path_join(path_b, sizeof(path_b), compare->dir, b->path))
        return false;
    File *first = storage_file_alloc(compare->storage);
    File *second = storage_file_alloc(compare->storage);
    bool first_open = first && storage_file_open(first, path_a, FSAM_READ, FSOM_OPEN_EXISTING);
    bool second_open = second && storage_file_open(second, path_b, FSAM_READ, FSOM_OPEN_EXISTING);
    bool equal = first_open && second_open;
    if (equal)
        equal = storage_file_size(first) == a->size && storage_file_size(second) == b->size;
    uint32_t hash = 0;
    uint64_t offset = 0;
    uint8_t first_bytes[READ_BUFFER_SIZE], second_bytes[READ_BUFFER_SIZE];
    while (equal && offset < a->size) {
        size_t count = a->size - offset;
        if (count > READ_BUFFER_SIZE)
            count = READ_BUFFER_SIZE;
        equal = storage_file_read(first, first_bytes, count) == count &&
                storage_file_read(second, second_bytes, count) == count &&
                memcmp(first_bytes, second_bytes, count) == 0;
        if (equal) {
            hash = calculate_crc32(hash, first_bytes, count);
            offset += count;
        }
    }
    if (equal)
        equal = hash == a->hash && storage_file_size(first) == a->size &&
                storage_file_size(second) == b->size;
    if (first_open)
        storage_file_close(first);
    if (second_open)
        storage_file_close(second);
    if (first)
        storage_file_free(first);
    if (second)
        storage_file_free(second);
    return equal;
}

void storage_refresh_duplicates(SubDupFinderApp *app) {
    if (!scan_dir_is_valid(app->scanned_dir)) {
        app->db.num_groups = 0;
        return;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    FileCompareContext compare = {storage, app->scanned_dir};
    process_duplicates_verified(&app->db, files_equal, &compare);
    furi_record_close(RECORD_STORAGE);
}

bool storage_scan_directory(SubDupFinderApp *app, const char *dir, ScanStats *stats) {
    memset(stats, 0, sizeof(*stats));
    app->db.count = 0;
    app->db.num_groups = 0;

    if (!scan_dir_is_valid(dir))
        return false;

    if ((size_t)snprintf(app->scanned_dir, sizeof(app->scanned_dir), "%s", dir) >=
        sizeof(app->scanned_dir)) {
        return false;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *dir_file = storage_file_alloc(storage);
    bool opened = storage_dir_open(dir_file, dir);

    if (opened) {
        FileInfo file_info;
        char filename[256];
        char full_path[FULL_PATH_LEN];

        while (storage_dir_read(dir_file, &file_info, filename, sizeof(filename))) {
            if (file_info_is_dir(&file_info))
                continue;

            // storage_dir_read may have truncated the name into a full buffer; never guess
            // whether it was a .sub file, count it as too long instead.
            if (strlen(filename) == sizeof(filename) - 1) {
                stats->name_too_long++;
                continue;
            }

            if (!is_sub_file(filename))
                continue;

            uint32_t crc = 0;
            uint32_t size = 0;
            bool read_ok = path_join(full_path, sizeof(full_path), dir, filename) &&
                           file_crc32(storage, full_path, &crc, &size);

            if (scan_add_file(&app->db, stats, filename, size, crc, read_ok) == ScanAddStop)
                break;
        }
        storage_dir_close(dir_file);
        FileCompareContext compare = {storage, app->scanned_dir};
        process_duplicates_verified(&app->db, files_equal, &compare);
    }

    storage_file_free(dir_file);
    furi_record_close(RECORD_STORAGE);
    return opened;
}

StorageDeleteResult storage_delete_duplicate(SubDupFinderApp *app, const char *name) {
    if (!app || !scan_dir_is_valid(app->scanned_dir) || !file_name_valid(name))
        return StorageDeleteChanged;
    size_t selected = app->db.count;
    for (size_t i = 0; i < app->db.count; i++)
        if (!strcmp(app->db.records[i].path, name)) {
            selected = i;
            break;
        }
    if (selected == app->db.count)
        return StorageDeleteChanged;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    FileCompareContext compare = {storage, app->scanned_dir};
    bool confirmed = false;
    for (size_t g = 0; g < app->db.num_groups && !confirmed; g++) {
        const DuplicateGroup *group = &app->db.groups[g];
        if (group->count < 2 || group->start_index > selected ||
            group->start_index > app->db.count ||
            group->count > app->db.count - group->start_index ||
            selected - group->start_index >= group->count)
            continue;
        for (size_t i = group->start_index; i < group->start_index + group->count; i++) {
            if (i != selected &&
                files_equal(&app->db.records[selected], &app->db.records[i], &compare)) {
                confirmed = true;
                break;
            }
        }
    }
    char path[FULL_PATH_LEN];
    StorageDeleteResult result = StorageDeleteChanged;
    if (confirmed && path_join(path, sizeof(path), app->scanned_dir, name))
        result = storage_simply_remove(storage, path) ? StorageDeleteOk : StorageDeleteFailed;
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool storage_list_folders(const char *dir, char names[][APP_MAX_PATH_LEN], size_t max_names,
                          size_t *count, size_t *overflow) {
    *count = 0;
    *overflow = 0;

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *dir_file = storage_file_alloc(storage);
    bool opened = storage_dir_open(dir_file, dir);

    if (opened) {
        FileInfo file_info;
        // One byte larger than the record field, so a boundary and a truncated name differ.
        char name[APP_MAX_PATH_LEN + 1];

        while (storage_dir_read(dir_file, &file_info, name, sizeof(name))) {
            if (!file_info_is_dir(&file_info))
                continue;

            switch (browse_decide_entry(name, *count, max_names)) {
                case BrowseEntrySkip:
                    break;
                case BrowseEntryOverflow:
                    (*overflow)++;
                    break;
                case BrowseEntryAdd:
                    // Explicit precision, not "%s": browse_decide_entry already bounds name to
                    // APP_MAX_PATH_LEN - 1, but the compiler can't see across that call.
                    snprintf(names[*count], APP_MAX_PATH_LEN, "%.*s", APP_MAX_PATH_LEN - 1, name);
                    (*count)++;
                    break;
            }
        }
        storage_dir_close(dir_file);
    }

    storage_file_free(dir_file);
    furi_record_close(RECORD_STORAGE);
    return opened;
}
