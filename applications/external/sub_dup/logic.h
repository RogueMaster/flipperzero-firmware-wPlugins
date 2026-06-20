#ifndef LOGIC_H
#define LOGIC_H

#include <stddef.h>
#include <stdint.h>

#define MAX_FILES        128
#define APP_MAX_PATH_LEN 64

typedef struct {
    char path[APP_MAX_PATH_LEN];
    uint32_t hash;
} FileRecord;

typedef struct {
    uint32_t hash;
    size_t start_index;
    size_t count;
} DuplicateGroup;

typedef struct {
    FileRecord records[MAX_FILES];
    size_t count;
    DuplicateGroup groups[MAX_FILES];
    size_t num_groups;
} HashDatabase;

uint32_t calculate_crc32(uint32_t crc, const uint8_t* data, size_t size);
void process_duplicates(HashDatabase* db);
void db_remove_record(HashDatabase* db, const char* filename);

#endif
