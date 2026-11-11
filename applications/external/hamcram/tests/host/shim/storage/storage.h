/* Storage API subset backed by the host file system */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Storage Storage;
typedef struct File File;

typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2,
    FSAM_READ_WRITE = 3,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_OPEN_ALWAYS = 2,
    FSOM_OPEN_APPEND = 4,
    FSOM_CREATE_NEW = 8,
    FSOM_CREATE_ALWAYS = 16,
} FS_OpenMode;

typedef enum {
    FSE_OK,
    FSE_NOT_EXIST,
    FSE_INTERNAL,
} FS_Error;

File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode);
bool storage_file_close(File* file);
size_t storage_file_read(File* file, void* buffer, size_t size);
size_t storage_file_write(File* file, const void* buffer, size_t size);
bool storage_file_seek(File* file, uint32_t offset, bool from_start);
uint64_t storage_file_size(File* file);
FS_Error storage_common_remove(Storage* storage, const char* path);
FS_Error storage_common_rename(Storage* storage, const char* old_path, const char* new_path);

/* Test hook: the next `count` renames fail, as if power was lost mid-save */
void shim_storage_fail_renames(int count);
