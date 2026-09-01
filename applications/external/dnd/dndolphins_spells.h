#pragma once

#include "dnd_data.h"
#include "dnd_spell_eligibility.h"
#include "dnd_storage.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PocketSpellCastCantrip,
    PocketSpellCastFree,
    PocketSpellCastSlot,
    PocketSpellCastPact,
    PocketSpellCastPoints,
    PocketSpellCastRitual,
} PocketSpellCastResource;

typedef struct {
    uint8_t level;
    uint8_t resource;
    uint8_t class_index;
} PocketSpellCastOption;

#define POCKET_D20_MAX_SPELL_CAST_OPTIONS 24U

typedef struct {
    uint8_t known[POCKET_D20_MAX_CLASSES];
    uint8_t prepared[POCKET_D20_MAX_CLASSES];
} DndDolphinsSpellClassCounts;

uint8_t dndolphins_spells_casting_ability_for(
    const PocketCharacter* character, const PocketSpell* spell);
int8_t dndolphins_spells_attack_modifier(const PocketCharacter* character);
int8_t dndolphins_spells_save_dc(const PocketCharacter* character);
int8_t dndolphins_spells_attack_modifier_for(
    const PocketCharacter* character, const PocketSpell* spell);
int8_t dndolphins_spells_save_dc_for(
    const PocketCharacter* character, const PocketSpell* spell);

void dndolphins_spells_recalculate_multiclass_slots(PocketCharacter* character);
bool dndolphins_spells_refresh_class_spellcasting(PocketClassLevel* class_level);
bool dndolphins_spells_apply_level_progression(PocketCharacter* character, uint8_t class_index);
bool dndolphins_spells_initialize_spell_slots_if_unset(PocketCharacter* character);
uint8_t dndolphins_spells_point_cost(uint8_t level);

bool dndolphins_spells_is_tracked(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared);
bool dndolphins_spells_can_ritual(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared);
bool dndolphins_spells_record_has_cast_resource(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current);
uint8_t dndolphins_spells_build_cast_options(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    PocketSpellCastOption* options,
    uint8_t capacity);

bool dndolphins_spells_class_counts(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellClassCounts* counts,
    uint8_t* total_count);

bool dndolphins_spells_collect_combat_indices(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count);

bool dndolphins_spells_collect_ritual_indices(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count);
