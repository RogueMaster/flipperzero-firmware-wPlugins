#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Storage Storage;
typedef struct File File;

enum { FSAM_READ = 1, FSOM_OPEN_EXISTING = 1 };
File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, uint8_t access, uint8_t mode);
uint16_t storage_file_read(File* file, void* buffer, uint16_t size);
void storage_file_close(File* file);
