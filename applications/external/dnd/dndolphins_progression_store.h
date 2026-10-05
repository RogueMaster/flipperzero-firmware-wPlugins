#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

#define DND_PROGRESS_CACHE_SIZE 8U
#define DND_PROGRESS_PAGE_COUNT 32U

void dndolphins_progression_store_feature_path(char* out, size_t size, uint32_t profile);
void dndolphins_progression_store_applied_path(char* out, size_t size, uint32_t profile);

typedef enum {
    DndFeatureRechargeTurn,
    DndFeatureRechargeEncounter,
    DndFeatureRechargeShortRest,
    DndFeatureRechargeLongRest,
} DndFeatureRechargeEvent;

bool dndolphins_progression_store_features_exist(Storage* storage, uint32_t profile);
bool dndolphins_progression_store_features_count(
    Storage* storage,
    uint32_t profile,
    uint16_t* total_count);
bool dndolphins_progression_store_features_find_name(
    Storage* storage,
    uint32_t profile,
    const char* name,
    DndFeature* feature_out,
    bool* found);
bool dndolphins_progression_store_features_contains_name(
    Storage* storage,
    uint32_t profile,
    const char* name,
    bool* found);
bool dndolphins_progression_store_features_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count);
bool dndolphins_progression_store_features_load_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_PROGRESS_PAGE_COUNT],
    uint8_t* valid_pages);
bool dndolphins_progression_store_features_save_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character);
bool dndolphins_progression_store_features_append(
    Storage* storage,
    uint32_t profile,
    const DndFeature* feature);
bool dndolphins_progression_store_features_delete(
    Storage* storage,
    uint32_t profile,
    uint16_t logical_index);
bool dndolphins_progression_store_features_recharge(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    DndFeatureRechargeEvent event);
bool dndolphins_progression_store_features_remap_classes(
    Storage* storage,
    uint32_t profile,
    uint8_t removed_class);

bool dndolphins_progression_store_applied_exists(
    Storage* storage,
    uint32_t profile,
    const char* stable_id);
bool dndolphins_progression_store_mark_applied(
    Storage* storage,
    uint32_t profile,
    const char* stable_id);

bool dndolphins_progression_store_delete_sidecars(Storage* storage, uint32_t profile);
bool dndolphins_progression_store_copy_sidecars(
    Storage* storage,
    uint32_t source_profile,
    uint32_t destination_profile);
