#pragma once
#include "dnd_data.h"
#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define DND_BACKUP_CORE_NAME_LEN 128U

bool dnd_backup_storage_valid_directory(const char* path);
bool dnd_backup_storage_export_bundle(
    Storage* storage,
    uint32_t profile,
    const char* destination_dir);
bool dnd_backup_storage_restore_bundle(
    Storage* storage,
    uint32_t profile,
    const char* core_shd_path,
    DndSaveData* restored);
uint8_t dnd_backup_storage_list_core_files(
    Storage* storage,
    uint32_t profile,
    const char* directory,
    char names[][DND_BACKUP_CORE_NAME_LEN],
    uint8_t capacity,
    uint16_t* total);
