#include "log_manager.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* filename;
    int index;
} LogFile;

static void sort_log_files(LogFile* files, size_t count) {
    // Simple bubble sort implementation
    for(size_t i = 0; i < count - 1; i++) {
        for(size_t j = 0; j < count - i - 1; j++) {
            if(files[j].index < files[j + 1].index) {
                // Swap the elements
                LogFile temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
}

static bool parse_log_index(const char* filename, const char* prefix, int* index) {
    // Expected format: prefix_NUMBER.txt
    size_t prefix_len = strlen(prefix);
    if(strncmp(filename, prefix, prefix_len) != 0) return false;

    const char* num_start = filename + prefix_len;
    if(*num_start != '_') return false;
    num_start++;

    char* end;
    *index = strtol(num_start, &end, 10);

    // Debug print to see what's happening
    FURI_LOG_D("LogManager", "Parsing '%s': num_start='%s', end='%s'", filename, num_start, end);

    // Check for valid number and .txt extension
    if(*index < 0 || (end == num_start) || // Invalid number
       strncmp(end, ".txt", 4) != 0 || // Must end with .txt
       *(end + 4) != '\0') { // Nothing should follow .txt
        return false;
    }

    return true;
}

bool get_latest_log_file(Storage* storage, const char* dir, const char* prefix, char* out_path) {
    if(!storage || !dir || !prefix || !out_path) return false;

    File* dir_handle = storage_file_alloc(storage);
    if(!storage_dir_open(dir_handle, dir)) {
        storage_file_free(dir_handle);
        return false;
    }

    LogFile* files = NULL;
    size_t files_count = 0;
    size_t files_capacity = 16;
    files = malloc(sizeof(LogFile) * files_capacity);

    FileInfo file_info;
    char* filename = malloc(MAX_FILENAME_LEN);

    // Collect all matching log files
    while(storage_dir_read(dir_handle, &file_info, filename, MAX_FILENAME_LEN)) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        int index;
        if(!parse_log_index(filename, prefix, &index)) continue;

        if(files_count >= files_capacity) {
            files_capacity *= 2;
            LogFile* new_files = realloc(files, sizeof(LogFile) * files_capacity);
            if(!new_files) {
                for(size_t j = 0; j < files_count; j++) {
                    free(files[j].filename);
                }
                free(files);
                free(filename);
                storage_dir_close(dir_handle);
                storage_file_free(dir_handle);
                return false;
            }
            files = new_files;
        }

        files[files_count].filename = strdup(filename);
        files[files_count].index = index;
        files_count++;
    }

    bool result = false;
    if(files_count > 0) {
        // Sort files by index (descending order)
        sort_log_files(files, files_count);

        // Get the latest file (highest index)
        snprintf(out_path, MAX_FILENAME_LEN, "%s/%s", dir, files[0].filename);
        result = true;
    }

    // Cleanup
    for(size_t i = 0; i < files_count; i++) {
        free(files[i].filename);
    }
    free(files);
    free(filename);
    storage_dir_close(dir_handle);
    storage_file_free(dir_handle);

    if(result) {
        File* test_file = storage_file_alloc(storage);
        if(!storage_file_open(test_file, out_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_W("LogManager", "Latest log file cannot be opened: %s", out_path);
            result = false;
        }
        storage_file_close(test_file);
        storage_file_free(test_file);
    }

    return result;
}
