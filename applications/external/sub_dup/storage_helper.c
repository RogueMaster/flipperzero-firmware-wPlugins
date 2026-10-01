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

bool storage_scan_directory(SubDupFinderApp *app, const char *dir, ScanStats *stats) {
    memset(stats, 0, sizeof(*stats));
    app->db.count = 0;
    app->db.num_groups = 0;

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
        process_duplicates(&app->db);
    }

    storage_file_free(dir_file);
    furi_record_close(RECORD_STORAGE);
    return opened;
}

bool storage_delete_file(const char *path) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return ok;
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
