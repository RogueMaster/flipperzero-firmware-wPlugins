#pragma once

#include <furi.h>
#include <storage/storage.h>

#define WEEBO_MAX_NFC_FILES 200
#define NFC_APP_EXTENSION   ".nfc"

typedef struct WeeboFileList WeeboFileList;

typedef enum {
    WeeboCycleDirectionNext,
    WeeboCycleDirectionPrev,
} WeeboCycleDirection;

WeeboFileList* weebo_file_list_alloc();
void weebo_file_list_free(WeeboFileList* file_list);

void weebo_file_list_reset(WeeboFileList* file_list);
bool weebo_file_list_scan(WeeboFileList* file_list, Storage* storage, const char* directory);

size_t weebo_file_list_get_count(WeeboFileList* file_list);
const char* weebo_file_list_get_current_path(WeeboFileList* file_list);
size_t weebo_file_list_get_current_index(WeeboFileList* file_list);
bool weebo_file_list_set_current_path(WeeboFileList* file_list, const char* path);

bool weebo_file_list_cycle(WeeboFileList* file_list, WeeboCycleDirection direction);
