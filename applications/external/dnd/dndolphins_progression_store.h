#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

#define DND_PROGRESS_CACHE_SIZE 8U

void dndolphins_progression_store_feature_path(char* out, size_t size, uint32_t profile);
void dndolphins_progression_store_applied_path(char* out, size_t size, uint32_t profile);

typedef enum {
    DndFeatureRechargeTurn,
    DndFeatureRechargeEncounter,
    DndFeatureRechargeShortRest,
    DndFeatureRechargeLongRest,
} DndFeatureRechargeEvent;

bool dndolphins_progression_store_features_exist(Storage* storage, uint32_t profile);
bool dndolphins_progression_store_features_count(Storage* storage, uint32_t profile, uint8_t* total_count);
bool dndolphins_progression_store_features_contains_name(
    Storage* storage, uint32_t profile, const char* name, bool* found);
bool dndolphins_progression_store_features_load_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    PocketCharacter* character,
    uint8_t* total_count);
bool dndolphins_progression_store_features_save_window(
    Storage* storage,
    uint32_t profile,
    uint8_t start,
    const PocketCharacter* character);
bool dndolphins_progression_store_features_append(
    Storage* storage,
    uint32_t profile,
    const PocketFeature* feature);
bool dndolphins_progression_store_features_delete(Storage* storage, uint32_t profile, uint8_t logical_index);
bool dndolphins_progression_store_features_recharge(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    DndFeatureRechargeEvent event);
bool dndolphins_progression_store_features_remap_classes(
    Storage* storage,
    uint32_t profile,
    uint8_t removed_class);

bool dndolphins_progression_store_applied_exists(Storage* storage, uint32_t profile, const char* stable_id);
bool dndolphins_progression_store_mark_applied(Storage* storage, uint32_t profile, const char* stable_id);

bool dndolphins_progression_store_delete_sidecars(Storage* storage, uint32_t profile);
bool dndolphins_progression_store_copy_sidecars(Storage* storage, uint32_t source_profile, uint32_t destination_profile);
