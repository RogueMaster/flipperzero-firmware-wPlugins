#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

typedef struct {
    uint32_t id;
    uint8_t level;
    char name[DND_CHARACTER_NAME_LEN];
} DndProfileEntry;

#define DND_STORAGE_PROFILE_CACHE_SIZE    8U
#define DND_STORAGE_COLLECTION_CACHE_SIZE 8U
#define DND_STORAGE_COLLECTION_PAGE_COUNT 32U
#define DND_INVENTORY_BAG_NAME_LEN        24U

typedef struct {
    uint32_t active_profile;
    uint16_t count;
    uint16_t cache_start;
    uint8_t cache_count;
    uint8_t active_entry_valid;
    DndProfileEntry entries[DND_STORAGE_PROFILE_CACHE_SIZE];
    DndProfileEntry active_entry;
    uint32_t highest_reserved_id;
    uint8_t reserved_id_seen;
    uint8_t character_file_seen;
    uint8_t scan_succeeded;
} DndProfileState;

bool dnd_storage_move_legacy_profiles(Storage* storage);
void dnd_storage_profiles_set_defaults(DndProfileState* profiles);
void dnd_storage_profiles_free(DndProfileState* profiles);
bool dnd_storage_profiles_load(Storage* storage, DndProfileState* profiles);
bool dnd_storage_profiles_save(Storage* storage, const DndProfileState* profiles);
bool dnd_storage_profiles_refresh(Storage* storage, DndProfileState* profiles);
const DndProfileEntry*
    dnd_storage_profiles_entry_at(Storage* storage, DndProfileState* profiles, uint16_t index);
bool dnd_storage_profiles_window(Storage* storage, DndProfileState* profiles, uint16_t start);
bool dnd_storage_profiles_find(Storage* storage, uint32_t profile, DndProfileEntry* entry);
bool dnd_storage_profiles_next_after(Storage* storage, uint32_t profile, DndProfileEntry* entry);
uint32_t dnd_storage_profiles_next_id(const DndProfileState* profiles);

/* Character-owned spell/item collections. These files are authoritative for owned
   records; the main character save does not contain spell/item rows. */
typedef bool (*DndDolphinsSpellRecordVisitor)(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context);

typedef bool (
    *DndDolphinsItemRecordVisitor)(uint16_t logical_index, const DndItem* item, void* context);

bool dnd_storage_visit_spells(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellRecordVisitor visitor,
    void* context,
    uint16_t* total_count);

/* Inventory bags. Main is the legacy inventory_<id>.txt sidecar. Group uses
   invGroup_<id>.txt; additional bags use inv<safeBagName>_<id>.txt. Bag lists
   are streamed from filenames/header metadata rather than retained in RAM. */
bool dnd_storage_inventory_bag_at(
    Storage* storage,
    uint32_t profile,
    uint8_t index,
    char* name,
    size_t size);
uint8_t dnd_storage_inventory_bag_count(Storage* storage, uint32_t profile);
bool dnd_storage_inventory_bag_create(Storage* storage, uint32_t profile, const char* name);
bool dnd_storage_inventory_bag_delete(Storage* storage, uint32_t profile, const char* name);
bool dnd_storage_visit_items_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    DndDolphinsItemRecordVisitor visitor,
    void* context,
    uint16_t* total_count);
bool dnd_storage_load_items_window_indexed_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages);
bool dnd_storage_items_exist_bag(Storage* storage, uint32_t profile, const char* bag);
bool dnd_storage_save_items_window_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    uint16_t start,
    const DndCharacter* character);
bool dnd_storage_append_item_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    const DndItem* item);
bool dnd_storage_delete_item_bag(
    Storage* storage,
    uint32_t profile,
    const char* bag,
    const DndCharacter* owner,
    uint16_t index);
bool dnd_storage_move_items_bag_selected(
    Storage* storage,
    uint32_t profile,
    const char* source_bag,
    const char* destination_bag,
    const DndCharacter* owner,
    const uint8_t* selected_bits,
    uint16_t source_total,
    uint16_t* moved_count);

bool dnd_storage_visit_items(
    Storage* storage,
    uint32_t profile,
    DndDolphinsItemRecordVisitor visitor,
    void* context,
    uint16_t* total_count);
/* Indexed collection-window loaders. The first uncached load scans the small
   sidecar once to learn total count and page offsets; later page loads seek
   directly to an aligned eight-record page. Callers invalidate valid_pages
   after a successful rewrite because line offsets may have changed. */
bool dnd_storage_load_spellbook_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
    uint8_t* valid_pages);
bool dnd_storage_load_items_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT],
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
    const DndCharacter* owner,
    const int32_t currency[5]);

typedef struct {
    const char* path;
    const char* match;
} DndDolphinsItemSeedAsset;

/* Low-level item-sidecar composition. Feature policy (which assets/keys to use,
   when initialization occurs, and how currency is applied) belongs to items.c.
   currency_total is both the starting balance and the persisted final balance. */
bool dnd_storage_create_items_from_assets(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndDolphinsItemSeedAsset* assets,
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
    const DndCharacter* owner,
    const DndDolphinsItemSeedAsset* assets,
    uint8_t asset_count,
    const DndDolphinsItemSeedAsset* fallback_asset,
    int32_t currency_total[5],
    bool* applied);

bool dnd_storage_load_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count);
bool dnd_storage_save_spellbook_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character);
bool dnd_storage_append_spell(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max);
bool dnd_storage_delete_spell(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint16_t index);
bool dnd_storage_reset_spell_free_casts(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner);
bool dnd_storage_remap_spell_classes(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint8_t removed_class);

bool dnd_storage_load_items_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count);
bool dnd_storage_save_items_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character);
bool dnd_storage_append_item(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndItem* item);
bool dnd_storage_append_items(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    const DndItem* items,
    uint8_t count);
bool dnd_storage_delete_item(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* owner,
    uint16_t index);

/* Resolve the current canonical profile filename for a profile id. Read-only
   projections use this to stream only fields they own/need. */
bool dnd_storage_find_profile_path(Storage* storage, uint32_t profile, char* output, size_t size);
bool dnd_storage_validate_character_path(Storage* storage, const char* path);

bool dnd_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    DndSaveData* data,
    bool* recovered_backup);
bool dnd_storage_save_profile(Storage* storage, uint32_t profile, const DndSaveData* data);
bool dnd_storage_save_profile_updated(Storage* storage, uint32_t profile, const DndSaveData* data);
bool dnd_storage_save_profile_known_updated(
    Storage* storage,
    const DndProfileEntry* current_entry,
    const DndSaveData* data);
bool dnd_storage_delete_profile(Storage* storage, uint32_t profile);
bool dnd_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination);
bool dnd_storage_archive_profile(Storage* storage, uint32_t profile);
bool dnd_storage_verify_profile(Storage* storage, uint32_t profile);
bool dnd_storage_validate_profile_semantics(Storage* storage, uint32_t profile);
bool dnd_storage_recover_profile_backup(Storage* storage, uint32_t profile, DndSaveData* data);
/* Level-tagged SHD history. Core character state is restored to the canonical
   character file while Inventory, Spellbook and progression sidecars are
   restored to their respective live files. */
uint8_t dnd_storage_list_shd_levels(
    Storage* storage,
    uint32_t profile,
    uint8_t* levels,
    uint8_t capacity);
bool dnd_storage_restore_shd(Storage* storage, uint32_t profile, uint8_t level, DndSaveData* data);
bool dnd_storage_restore_shd_path(
    Storage* storage,
    uint32_t profile,
    const char* core_snapshot,
    DndSaveData* data);
