#pragma once

#include "dnd_data.h"
#include "dnd_spell_eligibility.h"
#include "dnd_storage.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DndSpellCastCantrip,
    DndSpellCastFree,
    DndSpellCastSlot,
    DndSpellCastPact,
    DndSpellCastPoints,
    DndSpellCastRitual,
} DndSpellCastResource;

typedef struct {
    uint8_t level;
    uint8_t resource;
    uint8_t class_index;
} DndSpellCastOption;

#define DNDOLPHINS_MAX_SPELL_CAST_OPTIONS 24U

typedef struct {
    uint16_t known[DND_MAX_CLASSES];
    uint16_t prepared[DND_MAX_CLASSES];
    uint16_t granted[DND_MAX_CLASSES];
} DndDolphinsSpellClassCounts;

uint8_t
    dndolphins_spells_casting_ability_for(const DndCharacter* character, const DndSpell* spell);
int8_t dndolphins_spells_attack_modifier(const DndCharacter* character);
int8_t dndolphins_spells_save_dc(const DndCharacter* character);
int8_t dndolphins_spells_attack_modifier_for(const DndCharacter* character, const DndSpell* spell);
int8_t dndolphins_spells_save_dc_for(const DndCharacter* character, const DndSpell* spell);

void dndolphins_spells_recalculate_shared_slots(
    const DndClassLevel* classes,
    uint8_t class_count,
    uint8_t spell_slots_current[DND_SLOT_COUNT],
    uint8_t spell_slots_max[DND_SLOT_COUNT]);
void dndolphins_spells_recalculate_multiclass_slots(DndCharacter* character);
bool dndolphins_spells_refresh_class_spellcasting(DndClassLevel* class_level);
bool dndolphins_spells_apply_level_progression(DndCharacter* character, uint8_t class_index);
bool dndolphins_spells_initialize_spell_slots_if_unset(DndCharacter* character);
uint8_t dndolphins_spells_point_cost(uint8_t level);

bool dndolphins_spells_is_tracked(const DndSpell* spell, uint8_t known, uint8_t always_prepared);
bool dndolphins_spells_can_ritual(const DndSpell* spell, uint8_t known, uint8_t always_prepared);
bool dndolphins_spells_record_has_cast_resource(
    const DndCharacter* character,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current);
uint8_t dndolphins_spells_build_cast_options(
    const DndCharacter* character,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    DndSpellCastOption* options,
    uint8_t capacity);

bool dndolphins_spells_class_counts(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellClassCounts* counts,
    uint16_t* total_count);

bool dndolphins_spells_collect_combat_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count);

bool dndolphins_spells_collect_utility_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count);

bool dndolphins_spells_collect_ritual_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count);
