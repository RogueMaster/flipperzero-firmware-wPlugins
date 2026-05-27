#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Storage Storage;
typedef struct File File;
typedef struct FileInfo FileInfo;

typedef enum {
    FSE_OK = 0,
    FSE_NOT_EXIST = 1,
} FS_Error;

typedef enum {
    FSAM_READ = (1 << 0),
    FSAM_WRITE = (1 << 1),
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_OPEN_ALWAYS = 2,
    FSOM_OPEN_APPEND = 4,
    FSOM_CREATE_ALWAYS = 16,
} FS_OpenMode;

bool file_info_is_dir(const FileInfo *info);
FS_Error storage_common_remove(Storage *storage, const char *path);
FS_Error storage_common_copy(Storage *storage, const char *old_path, const char *new_path);
FS_Error storage_common_rename(Storage *storage, const char *old_path, const char *new_path);
bool storage_dir_exists(Storage *storage, const char *path);
bool storage_simply_mkdir(Storage *storage, const char *path);
bool storage_file_exists(Storage *storage, const char *path);
bool storage_dir_open(File *file, const char *path);
bool storage_dir_read(File *file, FileInfo *info, char *name, size_t name_size);
void storage_dir_close(File *file);
File *storage_file_alloc(Storage *storage);
void storage_file_free(File *file);
bool storage_file_open(File *file, const char *path, FS_AccessMode access_mode,
                       FS_OpenMode open_mode);
size_t storage_file_size(File *file);
size_t storage_file_read(File *file, void *buffer, size_t size);
size_t storage_file_write(File *file, const void *buffer, size_t size);
void storage_file_close(File *file);
