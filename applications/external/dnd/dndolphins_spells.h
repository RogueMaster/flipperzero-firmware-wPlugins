#pragma once

#include "dndolphins.h"
#include "dndolphins_storage.h"

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

uint8_t pocket_d20_spell_casting_ability_for(
    const PocketCharacter* character, const PocketSpell* spell);
int8_t pocket_d20_spell_attack_modifier(const PocketCharacter* character);
int8_t pocket_d20_spell_save_dc(const PocketCharacter* character);
int8_t pocket_d20_spell_attack_modifier_for(
    const PocketCharacter* character, const PocketSpell* spell);
int8_t pocket_d20_spell_save_dc_for(
    const PocketCharacter* character, const PocketSpell* spell);

void pocket_d20_recalculate_multiclass_slots(PocketCharacter* character);
bool pocket_d20_apply_level_progression(PocketCharacter* character, uint8_t class_index);
bool pocket_d20_initialize_spell_slots_if_unset(PocketCharacter* character);
uint8_t pocket_d20_class_max_spell_level(const PocketClassLevel* class_level);
uint8_t pocket_d20_spell_point_cost(uint8_t level);

bool pocket_d20_spell_is_tracked(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared);
bool pocket_d20_spell_can_ritual(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared);
bool pocket_d20_spell_record_has_cast_resource(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current);
uint8_t pocket_d20_spells_build_cast_options(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    PocketSpellCastOption* options,
    uint8_t capacity);

bool pocket_d20_spells_collect_combat_indices(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count);
