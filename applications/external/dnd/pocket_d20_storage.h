#pragma once

#include "pocket_d20.h"

#include <storage/storage.h>

typedef struct {
    uint32_t id;
    uint8_t level;
    char name[POCKET_D20_NAME_LEN];
} PocketProfileEntry;

typedef struct {
    uint32_t active_profile;
    uint16_t count;
    uint16_t capacity;
    PocketProfileEntry* entries;
    uint8_t character_file_seen;
    uint8_t scan_succeeded;
} PocketProfileState;

void pocket_d20_profiles_set_defaults(PocketProfileState* profiles);
void pocket_d20_profiles_free(PocketProfileState* profiles);
bool pocket_d20_profiles_load(Storage* storage, PocketProfileState* profiles);
bool pocket_d20_profiles_save(Storage* storage, const PocketProfileState* profiles);
bool pocket_d20_profiles_refresh(Storage* storage, PocketProfileState* profiles);
uint32_t pocket_d20_profiles_next_id(const PocketProfileState* profiles);
bool pocket_d20_storage_migrate_legacy_profiles(Storage* storage, uint16_t* copied_files);

bool pocket_d20_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* recovered_backup);
bool pocket_d20_storage_save_profile(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data);
bool pocket_d20_storage_save_profile_known(
    Storage* storage,
    const PocketProfileEntry* current_entry,
    const PocketSaveData* data);
bool pocket_d20_storage_delete_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination);
bool pocket_d20_storage_export_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_archive_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_verify_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_restore_backup(Storage* storage, uint32_t profile, PocketSaveData* data);
bool pocket_d20_storage_rollback_migration(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data);
bool pocket_d20_storage_import_first(Storage* storage, uint32_t destination, PocketSaveData* data);
