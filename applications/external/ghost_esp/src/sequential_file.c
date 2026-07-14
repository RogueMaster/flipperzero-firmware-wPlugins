#include "sequential_file.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* sequential_file_resolve_path(
    Storage* storage,
    const char* dir,
    const char* prefix,
    const char* extension) {
    if(storage == NULL || dir == NULL || prefix == NULL || extension == NULL) {
        FURI_LOG_E("SequentialFile", "Invalid parameters passed to resolve_path");
        return NULL;
    }

    // Allocate a file handle for directory operations
    File* dir_handle = storage_file_alloc(storage);
    if(!dir_handle) {
        FURI_LOG_E("SequentialFile", "Failed to allocate file handle for directory");
        return NULL;
    }

    // Open the directory
    if(!storage_dir_open(dir_handle, dir)) {
        FURI_LOG_E("SequentialFile", "Failed to open directory: %s", dir);
        storage_file_free(dir_handle);
        return NULL;
    }

    FileInfo file_info;
    char filename[256];
    int highest_index = -1;
    size_t prefix_len = strlen(prefix);
    size_t extension_len = strlen(extension);

    // Iterate over files in the directory
    while(storage_dir_read(dir_handle, &file_info, filename, sizeof(filename))) {
        // Skip directories
        if(file_info.flags & FSF_DIRECTORY) continue;

        // Check if filename matches the expected pattern
        if(strncmp(filename, prefix, prefix_len) != 0) continue;
        size_t filename_len = strlen(filename);

        // Verify extension
        if(filename_len <= extension_len + 1) continue; // +1 for '.'
        if(strcmp(&filename[filename_len - extension_len], extension) != 0) continue;
        if(filename[filename_len - extension_len - 1] != '.') continue;

        // Extract the index part of the filename
        const char* index_start = filename + prefix_len;
        if(*index_start != '_') continue;
        index_start++; // Skip the '_'

        // Calculate the length of the index string
        size_t index_len = filename_len - prefix_len - extension_len - 2; // -2 for '_' and '.'

        // Copy the index string
        char index_str[32];
        if(index_len >= sizeof(index_str)) continue; // Prevent buffer overflow
        strncpy(index_str, index_start, index_len);
        index_str[index_len] = '\0';

        // Convert index string to integer
        char* endptr;
        long index = strtol(index_str, &endptr, 10);
        if(*endptr != '\0' || index < 0) continue; // Ensure full conversion and non-negative index

        if(index > highest_index) {
            highest_index = (int)index;
        }
    }

    storage_dir_close(dir_handle);
    storage_file_free(dir_handle);

    // Determine the new index
    int new_index = highest_index + 1;

    // Construct the new file path
    char file_path[256];
    int snprintf_result =
        snprintf(file_path, sizeof(file_path), "%s/%s_%d.%s", dir, prefix, new_index, extension);
    if(snprintf_result < 0 || (size_t)snprintf_result >= sizeof(file_path)) {
        FURI_LOG_E("SequentialFile", "snprintf failed or output truncated in resolve_path");
        return NULL;
    }

    FURI_LOG_I("SequentialFile", "Resolved new file path: %s", file_path);

    return strdup(file_path);
}

bool sequential_file_open(
    Storage* storage,
    File* file,
    const char* dir,
    const char* prefix,
    const char* extension) {
    if(storage == NULL || file == NULL || dir == NULL || prefix == NULL || extension == NULL) {
        FURI_LOG_E("SequentialFile", "Invalid parameters passed to open");
        return false;
    }

    char* file_path = sequential_file_resolve_path(storage, dir, prefix, extension);
    if(file_path == NULL) {
        FURI_LOG_E("SequentialFile", "Failed to resolve file path");
        return false;
    }

    // Open the file with the resolved path
    bool success = storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        FURI_LOG_I("SequentialFile", "Opened log file: %s", file_path);
    } else {
        FURI_LOG_E("SequentialFile", "Failed to open log file: %s", file_path);
    }

    free(file_path);
    return success;
}
