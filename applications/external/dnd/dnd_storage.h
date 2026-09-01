#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

typedef struct {
    uint32_t id;
    uint8_t level;
    char name[POCKET_D20_CHARACTER_NAME_LEN];
} PocketProfileEntry;

#define POCKET_D20_PROFILE_CACHE_SIZE    8U
#define POCKET_D20_COLLECTION_CACHE_SIZE 8U
#define POCKET_D20_COLLECTION_PAGE_COUNT 3U

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

bool dnd_storage_move_legacy_profiles(Storage* storage);
void dnd_storage_profiles_set_defaults(PocketProfileState* profiles);
void dnd_storage_profiles_free(PocketProfileState* profiles);
bool dnd_storage_profiles_load(Storage* storage, PocketProfileState* profiles);
bool dnd_storage_profiles_save(Storage* storage, const PocketProfileState* profiles);
bool dnd_storage_profiles_refresh(Storage* storage, PocketProfileState* profiles);
const PocketProfileEntry*
    dnd_storage_profiles_entry_at(Storage* storage, PocketProfileState* profiles, uint16_t index);
bool dnd_storage_profiles_window(Storage* storage, PocketProfileState* profiles, uint16_t start);
bool dnd_storage_profiles_find(Storage* storage, uint32_t profile, PocketProfileEntry* entry);
bool dnd_storage_profiles_next_after(Storage* storage, uint32_t profile, PocketProfileEntry* entry);
uint32_t dnd_storage_profiles_next_id(const PocketProfileState* profiles);

/* Character-owned spell/item collections. These files are authoritative for owned
   records; the main character save does not contain spell/item rows. */
typedef bool (*PocketD20SpellRecordVisitor)(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context);

typedef bool (
    *PocketD20ItemRecordVisitor)(uint8_t logical_index, const PocketItem* item, void* context);

bool dnd_storage_visit_spells(
    Storage* storage,
    uint32_t profile,
    PocketD20SpellRecordVisitor visitor,
    void* context,
    uint8_t* total_count);
bool dnd_storage_visit_items(
    Storage* storage,
    uint32_t profile,
    PocketD20ItemRecordVisitor visitor,
    void* context,
    uint8_t* total_count);
/* Indexed collection-window loaders. The first uncached load scans the small
   sidecar once to learn total count and page offsets; later page loads seek
   directly to an aligned eight-record page. Callers invalidate valid_pages
   after a successful rewrite because line offsets may have changed. */
bool dnd_storage_load_spellbook_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count,
    uint32_t page_offsets[POCKET_D20_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages);
bool dnd_storage_load_items_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count,
    uint32_t page_offsets[POCKET_D20_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages);
bool dnd_storage_items_exist(Storage* storage, uint32_t profile);
bool dnd_storage_remove_live_items(Storage* storage, uint32_t profile);
/* Currency is owned exclusively by the Inventory sidecar. Character profile
   Currency= fields are neither loaded nor serialized. */
bool dnd_storage_load_inventory_currency(
    Storage* storage,
    uint32_t profile,
    int32_t currency[5],
    bool* found);
bool dnd_storage_inventory_initial_grant_state(Storage* storage, uint32_t profile, uint8_t* state);
bool dnd_storage_inventory_initial_granted(Storage* storage, uint32_t profile, bool* granted);
bool dnd_storage_save_inventory_currency(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const int32_t currency[5]);

typedef struct {
    const char* path;
    const char* match;
} PocketD20ItemSeedAsset;

/* Low-level item-sidecar composition. Feature policy (which assets/keys to use,
   when initialization occurs, and how currency is applied) belongs to items.c.
   currency_total is both the starting balance and the persisted final balance. */
bool dnd_storage_create_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketD20ItemSeedAsset* assets,
    uint8_t asset_count,
    int32_t currency_total[5],
    bool* created);
/* Explicit one-time Inventory override helper. Existing records are preserved,
   matching seed records are appended, Currency= is increased, and a successful
   publish writes InitialInventory=2 so the override cannot be repeated. The
   returned currency_total is the exact balance committed to the sidecar. */
bool dnd_storage_regrant_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketD20ItemSeedAsset* assets,
    uint8_t asset_count,
    const PocketD20ItemSeedAsset* fallback_asset,
    int32_t currency_total[5],
    bool* applied);

bool dnd_storage_load_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count);
bool dnd_storage_save_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character);
bool dnd_storage_append_spell(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max);
bool dnd_storage_delete_spell(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    uint8_t index);
bool dnd_storage_reset_spell_free_casts(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner);
bool dnd_storage_remap_spell_classes(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    uint8_t removed_class);

bool dnd_storage_load_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count);
bool dnd_storage_save_items_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character);
bool dnd_storage_append_item(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    const PocketItem* item);
bool dnd_storage_delete_item(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* owner,
    uint8_t index);

/* Resolve the current canonical profile filename for a profile id. Read-only
   projections use this to stream only fields they own/need. */
bool dnd_storage_find_profile_path(Storage* storage, uint32_t profile, char* output, size_t size);

bool dnd_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* recovered_backup);
bool dnd_storage_save_profile(Storage* storage, uint32_t profile, const PocketSaveData* data);
bool dnd_storage_save_profile_updated(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data);
bool dnd_storage_save_profile_known_updated(
    Storage* storage,
    const PocketProfileEntry* current_entry,
    const PocketSaveData* data);
bool dnd_storage_delete_profile(Storage* storage, uint32_t profile);
bool dnd_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination);
bool dnd_storage_export_profile(Storage* storage, uint32_t profile);
bool dnd_storage_archive_profile(Storage* storage, uint32_t profile);
bool dnd_storage_verify_profile(Storage* storage, uint32_t profile);
bool dnd_storage_restore_backup(Storage* storage, uint32_t profile, PocketSaveData* data);
bool dnd_storage_import_first(Storage* storage, uint32_t destination, PocketSaveData* data);
