#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORAGE_APP_DATA_PATH_PREFIX "/current"
#define APP_DATA_PATH(name)          "/legacy/" name
#define RECORD_STORAGE               "storage"

typedef struct Storage Storage;
typedef struct File File;
typedef enum {
    FSE_OK,
    FSE_EXIST,
    FSE_NOT_EXIST,
    FSE_INTERNAL
} FS_Error;
typedef enum {
    FSAM_READ,
    FSAM_WRITE
} FS_AccessMode;
typedef enum {
    FSOM_OPEN_EXISTING,
    FSOM_CREATE_ALWAYS
} FS_OpenMode;

void* furi_record_open(const char* name);
void furi_record_close(const char* name);
File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode);
bool storage_file_close(File* file);
size_t storage_file_read(File* file, void* data, size_t size);
size_t storage_file_write(File* file, const void* data, size_t size);
bool storage_file_sync(File* file);
FS_Error storage_common_mkdir(Storage* storage, const char* path);
FS_Error storage_common_rename(Storage* storage, const char* from, const char* to);
FS_Error storage_common_remove(Storage* storage, const char* path);
