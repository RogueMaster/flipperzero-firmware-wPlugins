#pragma once

#include "dnd_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DndSpellResolutionNone,
    DndSpellResolutionAttack,
    DndSpellResolutionSave,
    DndSpellResolutionAutomatic,
    DndSpellResolutionTriggered,
    DndSpellResolutionHealing,
    DndSpellResolutionTemporaryHP,
    DndSpellResolutionMitigation,
    DndSpellResolutionTransfer,
    DndSpellResolutionVitality,
} DndSpellResolution;

typedef enum {
    DndSpellSecondaryNone,
    DndSpellSecondaryIndependent,
    DndSpellSecondaryAlternative,
    DndSpellSecondaryLater,
} DndSpellSecondaryRelation;

typedef enum {
    DndSpellDerivedNone,
    DndSpellDerivedHealHalfPrimary,
    DndSpellDerivedHealDoublePrimary,
} DndSpellDerivedEffect;

typedef enum {
    DndSpellSpecialNone,
    DndSpellSpecialSorcerousBurst,
} DndSpellSpecial;

typedef struct {
    uint8_t primary_dice;
    uint8_t primary_die;
    uint8_t secondary_dice;
    uint8_t secondary_die;
    int16_t flat_bonus;
    int16_t secondary_flat_bonus;
    uint8_t resolution;
    uint8_t secondary_resolution;
    uint8_t secondary_relation;
    uint8_t from_notes;
    uint8_t attack_rolls;
    uint8_t roll_instances;
    uint8_t derived_effect;
    uint8_t special;
} DndSpellDamageSpec;

bool dndolphins_spell_combat_damage_spec(
    const DndSpell* spell,
    uint8_t cast_level,
    uint8_t character_level,
    int8_t spellcasting_modifier,
    DndSpellDamageSpec* output);
