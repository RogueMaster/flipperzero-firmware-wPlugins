#pragma once
#include <furi.h>
#define RECORD_STORAGE "storage"
#define EXT_PATH(p)    "/ext/" p
typedef struct Storage Storage;
typedef struct File File;
/* FSE_OK is zero, so a truthiness test on an FS_Error is backwards */
typedef enum {
    FSE_OK = 0,
    FSE_NOT_READY,
    FSE_EXIST,
    FSE_NOT_EXIST,
    FSE_INVALID_PARAMETER,
    FSE_DENIED,
    FSE_INVALID_NAME,
    FSE_INTERNAL,
    FSE_NOT_IMPLEMENTED,
    FSE_ALREADY_OPEN,
} FS_Error;
typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2
} FS_AccessMode;
typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_OPENING_EXISTING = 1,
    FSOM_CREATE_ALWAYS = 4
} FS_OpenMode;
File* storage_file_alloc(Storage* s);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode am, FS_OpenMode om);
bool storage_file_close(File* f);
size_t storage_file_read(File* f, void* buf, size_t n);
size_t storage_file_write(File* f, const void* buf, size_t n);
FS_Error storage_common_mkdir(Storage* s, const char* path);
