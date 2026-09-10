#pragma once
#include <furi.h>
#define RECORD_STORAGE "storage"
#define EXT_PATH(p) "/ext/" p
typedef struct Storage Storage;
typedef struct File File;
typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_OPENING_EXISTING = 1, FSOM_CREATE_ALWAYS = 4 } FS_OpenMode;
File* storage_file_alloc(Storage* s);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode am, FS_OpenMode om);
bool storage_file_close(File* f);
uint16_t storage_file_read(File* f, void* buf, uint16_t n);
uint16_t storage_file_write(File* f, const void* buf, uint16_t n);
bool storage_common_mkdir(Storage* s, const char* path);
