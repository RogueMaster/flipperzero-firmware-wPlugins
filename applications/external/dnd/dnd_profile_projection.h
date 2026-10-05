#pragma once

#include "dnd_data.h"

#include <storage/storage.h>

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    char species[DND_NAME_LEN];
    char background[DND_NAME_LEN];
    uint8_t class_count;
    DndClassLevel classes[DND_MAX_CLASSES];
    int8_t ability_scores[DND_ABILITY_COUNT];
    int16_t armor_class;
    uint8_t exhaustion;
    uint8_t encumbrance_mode;
    int16_t carrying_capacity_override;
} DndInventoryProfileProjection;

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    uint8_t class_count;
    DndClassLevel classes[DND_MAX_CLASSES];
    int8_t ability_scores[DND_ABILITY_COUNT];
    uint8_t spellcasting_ability;
    int8_t spell_attack_misc;
    int8_t spell_save_misc;
    uint8_t arcane_recovery_used;
    uint8_t spell_slots_current[DND_SLOT_COUNT];
    uint8_t spell_slots_max[DND_SLOT_COUNT];
} DndSpellbookProfileProjection;

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    uint8_t class_count;
    uint8_t class_levels[DND_MAX_CLASSES];
    int8_t ability_scores[DND_ABILITY_COUNT];
    uint8_t skill_proficiency[DND_SKILL_COUNT];
    int8_t skill_misc[DND_SKILL_COUNT];
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
bool dnd_profile_projection_save_spellbook_magic(
    Storage* storage,
    uint32_t profile,
    const DndSpellbookProfileProjection* projection);
bool dnd_profile_projection_load_adventure(
    Storage* storage,
    uint32_t profile,
    DndAdventureProfileProjection* projection);
