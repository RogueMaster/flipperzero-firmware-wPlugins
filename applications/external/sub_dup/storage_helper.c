#include "storage_helper.h"
#include "logic.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define READ_BUFFER_SIZE 256

static uint32_t compute_file_hash(Storage* storage, const char* path) {
    uint32_t hash = 0;
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buffer[READ_BUFFER_SIZE];
        size_t read;
        while((read = storage_file_read(file, buffer, READ_BUFFER_SIZE)) > 0) {
            hash = calculate_crc32(hash, buffer, read);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return hash;
}

void storage_scan_directory(SubDupFinderApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    app->db.count = 0;

    if(storage_dir_open(dir, SCAN_DIR)) {
        FileInfo file_info;
        char filename[APP_MAX_PATH_LEN];
        char full_path[FULL_PATH_LEN];

        while(storage_dir_read(dir, &file_info, filename, sizeof(filename)) &&
              app->db.count < MAX_FILES) {
            popup_set_text(app->popup, filename, 64, 32, AlignCenter, AlignCenter);
            if(!file_info_is_dir(&file_info)) {
                snprintf(full_path, sizeof(full_path), "%s/%s", SCAN_DIR, filename);
                strncpy(
                    app->db.records[app->db.count].path,
                    filename,
                    sizeof(app->db.records[app->db.count].path) - 1);
                app->db.records[app->db.count].hash = compute_file_hash(storage, full_path);
                app->db.count++;
            }
        }
        storage_dir_close(dir);
        process_duplicates(&app->db);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

void storage_delete_file(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
}
