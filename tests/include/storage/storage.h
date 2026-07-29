#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_DATA_PATH(path) "/data/" path

typedef struct Storage Storage;
typedef struct File File;

typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_CREATE_ALWAYS = 16,
} FS_OpenMode;

typedef enum {
    FSE_OK,
    FSE_NOT_READY,
    FSE_EXIST,
    FSE_NOT_EXIST,
    FSE_INVALID_PARAMETER,
    FSE_DENIED,
    FSE_INVALID_NAME,
    FSE_INTERNAL,
} FS_Error;

File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode);
size_t storage_file_read(File* file, void* buffer, size_t size);
size_t storage_file_write(File* file, const void* buffer, size_t size);
uint64_t storage_file_size(File* file);
bool storage_file_close(File* file);
FS_Error storage_common_remove(Storage* storage, const char* path);
FS_Error storage_common_rename(Storage* storage, const char* old_path, const char* new_path);
