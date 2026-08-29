#pragma once

#include "dndolphins.h"

#include <storage/storage.h>

typedef struct {
    uint32_t id;
    uint8_t level;
    char name[POCKET_D20_CHARACTER_NAME_LEN];
} PocketProfileEntry;

#define POCKET_D20_PROFILE_CACHE_SIZE 8U
#define POCKET_D20_COLLECTION_CACHE_SIZE 8U

typedef struct {
    uint32_t active_profile;
    uint16_t count;
    uint16_t cache_start;
    uint8_t cache_count;
    uint8_t active_entry_valid;
    PocketProfileEntry entries[POCKET_D20_PROFILE_CACHE_SIZE];
    PocketProfileEntry active_entry;
    uint32_t highest_reserved_id;
    uint8_t reserved_id_seen;
    uint8_t character_file_seen;
    uint8_t scan_succeeded;
} PocketProfileState;

bool pocket_d20_storage_move_legacy_profiles(Storage* storage);
void pocket_d20_profiles_set_defaults(PocketProfileState* profiles);
void pocket_d20_profiles_free(PocketProfileState* profiles);
bool pocket_d20_profiles_load(Storage* storage, PocketProfileState* profiles);
bool pocket_d20_profiles_save(Storage* storage, const PocketProfileState* profiles);
bool pocket_d20_profiles_refresh(Storage* storage, PocketProfileState* profiles);
const PocketProfileEntry* pocket_d20_profiles_entry_at(
    Storage* storage,
    PocketProfileState* profiles,
    uint16_t index);
bool pocket_d20_profiles_window(
    Storage* storage,
    PocketProfileState* profiles,
    uint16_t start);
bool pocket_d20_profiles_find(
    Storage* storage,
    uint32_t profile,
    PocketProfileEntry* entry);
bool pocket_d20_profiles_next_after(
    Storage* storage,
    uint32_t profile,
    PocketProfileEntry* entry);
uint32_t pocket_d20_profiles_next_id(const PocketProfileState* profiles);

/* Character-owned spell/item collections. These files are authoritative for owned
   records; the main character save does not contain spell/item rows. */
bool pocket_d20_storage_save_spellbook(Storage* storage, uint32_t profile, const PocketCharacter* character);
typedef bool (*PocketD20SpellRecordVisitor)(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context);

typedef bool (*PocketD20ItemRecordVisitor)(
    uint8_t logical_index, const PocketItem* item, void* context);

bool pocket_d20_storage_visit_spells(
    Storage* storage,
    uint32_t profile,
    PocketD20SpellRecordVisitor visitor,
    void* context,
    uint8_t* total_count);
bool pocket_d20_storage_visit_items(
    Storage* storage,
    uint32_t profile,
    PocketD20ItemRecordVisitor visitor,
    void* context,
    uint8_t* total_count);

bool pocket_d20_storage_load_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count);
bool pocket_d20_storage_save_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character);
bool pocket_d20_storage_append_spell(
    Storage* storage,
    uint32_t profile,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max);
bool pocket_d20_storage_delete_spell(Storage* storage, uint32_t profile, uint8_t index);

bool pocket_d20_storage_save_items(Storage* storage, uint32_t profile, const PocketCharacter* character);
bool pocket_d20_storage_load_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count);
bool pocket_d20_storage_save_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character);
bool pocket_d20_storage_append_item(
    Storage* storage,
    uint32_t profile,
    const PocketItem* item);
bool pocket_d20_storage_delete_item(Storage* storage, uint32_t profile, uint8_t index);

bool pocket_d20_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* recovered_backup);
bool pocket_d20_storage_save_profile(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data);
bool pocket_d20_storage_save_profile_updated(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data);
bool pocket_d20_storage_save_profile_known_updated(
    Storage* storage,
    const PocketProfileEntry* current_entry,
    const PocketSaveData* data);
bool pocket_d20_storage_delete_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination);
bool pocket_d20_storage_export_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_archive_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_verify_profile(Storage* storage, uint32_t profile);
bool pocket_d20_storage_restore_backup(Storage* storage, uint32_t profile, PocketSaveData* data);
bool pocket_d20_storage_import_first(Storage* storage, uint32_t destination, PocketSaveData* data);
