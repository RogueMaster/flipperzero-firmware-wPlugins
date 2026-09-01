#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

typedef struct {
    char name[POCKET_D20_CHARACTER_NAME_LEN];
    char species[POCKET_D20_NAME_LEN];
    char background[POCKET_D20_NAME_LEN];
    uint8_t class_count;
    PocketClassLevel classes[POCKET_D20_MAX_CLASSES];
    int8_t ability_scores[POCKET_D20_ABILITY_COUNT];
    int16_t armor_class;
    uint8_t exhaustion;
    uint8_t encumbrance_mode;
    int16_t carrying_capacity_override;
} DndInventoryProfileProjection;

typedef struct {
    char name[POCKET_D20_CHARACTER_NAME_LEN];
    uint8_t class_count;
    PocketClassLevel classes[POCKET_D20_MAX_CLASSES];
} DndSpellbookProfileProjection;

typedef struct {
    char name[POCKET_D20_CHARACTER_NAME_LEN];
    uint8_t class_count;
    uint8_t class_levels[POCKET_D20_MAX_CLASSES];
    int8_t ability_scores[POCKET_D20_ABILITY_COUNT];
    uint8_t skill_proficiency[POCKET_D20_SKILL_COUNT];
    int8_t skill_misc[POCKET_D20_SKILL_COUNT];
} DndAdventureProfileProjection;

bool dnd_profile_projection_load_inventory(
    Storage* storage,
    uint32_t profile,
    DndInventoryProfileProjection* projection);
bool dnd_profile_projection_save_inventory_owned(
    Storage* storage,
    uint32_t profile,
    const DndInventoryProfileProjection* projection);
bool dnd_profile_projection_load_spellbook(
    Storage* storage,
    uint32_t profile,
    DndSpellbookProfileProjection* projection);
bool dnd_profile_projection_load_adventure(
    Storage* storage,
    uint32_t profile,
    DndAdventureProfileProjection* projection);
