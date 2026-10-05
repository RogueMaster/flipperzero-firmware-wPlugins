#include "dndolphins_spells.h"

#include "dnd_rules.h"
#include "dndolphins_spell_combat.h"

#include <string.h>

typedef struct {
    DndDolphinsSpellClassCounts* counts;
} DndDolphinsSpellCountContext;

static bool dndolphins_spells_count_record(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)logical_index;
    (void)free_casts_current;
    (void)free_casts_max;
    DndDolphinsSpellCountContext* count_context = context;
    if(!count_context || !count_context->counts || !spell || spell->class_index >= DND_MAX_CLASSES)
        return true;
    uint8_t class_index = spell->class_index;
    if(known && count_context->counts->known[class_index] < UINT16_MAX)
        ++count_context->counts->known[class_index];
    if((spell->prepared || always_prepared) &&
       count_context->counts->prepared[class_index] < UINT16_MAX)
        ++count_context->counts->prepared[class_index];
    /* `granted` means spells supplied outside the class's normal spell capacity.
       Wizard level-up spellbook choices use stable IDs for auditability, but they
       are already represented by spellbook_size and must not be double-counted
       as free/granted spells in the Magic totals. */
    bool normal_capacity_learning = spell->stable_id[0] &&
                                    (!strncmp(spell->stable_id, "wiz-l", 5U) ||
                                     !strncmp(spell->stable_id, "ek-", 3U) ||
                                     !strncmp(spell->stable_id, "at-", 3U));
    if(known && !normal_capacity_learning && (spell->stable_id[0] || spell->grant_name[0]) &&
       count_context->counts->granted[class_index] < UINT16_MAX)
        ++count_context->counts->granted[class_index];
    return true;
}

bool dndolphins_spells_class_counts(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellClassCounts* counts,
    uint16_t* total_count) {
    if(!storage || !counts) return false;
    memset(counts, 0, sizeof(*counts));
    DndDolphinsSpellCountContext context = {.counts = counts};
    return dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_count_record, &context, total_count);
}

uint8_t
    dndolphins_spells_casting_ability_for(const DndCharacter* character, const DndSpell* spell) {
    if(spell) {
        uint8_t class_index = spell->class_index;
        if(class_index < character->class_count &&
           character->classes[class_index].spellcasting_mode != DndSpellcastingNone &&
           character->classes[class_index].spellcasting_ability < DND_ABILITY_COUNT)
            return character->classes[class_index].spellcasting_ability;
    }
    return character->spellcasting_ability < DND_ABILITY_COUNT ? character->spellcasting_ability :
                                                                 DndAbilityIntelligence;
}

int8_t
    dndolphins_spells_attack_modifier_for(const DndCharacter* character, const DndSpell* spell) {
    uint8_t ability = dndolphins_spells_casting_ability_for(character, spell);
    return (int8_t)(dnd_rules_core_ability_modifier(character->ability_scores[ability]) +
                    dnd_rules_core_proficiency_bonus(character) + character->spell_attack_misc +
                    dnd_rules_core_exhaustion_penalty(character));
}

int8_t dndolphins_spells_save_dc_for(const DndCharacter* character, const DndSpell* spell) {
    uint8_t ability = dndolphins_spells_casting_ability_for(character, spell);
    return (int8_t)(8 + dnd_rules_core_ability_modifier(character->ability_scores[ability]) +
                    dnd_rules_core_proficiency_bonus(character) + character->spell_save_misc);
}

int8_t dndolphins_spells_attack_modifier(const DndCharacter* character) {
    return dndolphins_spells_attack_modifier_for(character, NULL);
}

int8_t dndolphins_spells_save_dc(const DndCharacter* character) {
    return dndolphins_spells_save_dc_for(character, NULL);
}

void dndolphins_spells_recalculate_shared_slots(
    const DndClassLevel* classes,
    uint8_t class_count,
    uint8_t spell_slots_current[DND_SLOT_COUNT],
    uint8_t spell_slots_max[DND_SLOT_COUNT]) {
    if(!classes || !spell_slots_current || !spell_slots_max) return;
    static const uint8_t slots[20][9] = {
        {2, 0, 0, 0, 0, 0, 0, 0, 0}, {3, 0, 0, 0, 0, 0, 0, 0, 0}, {4, 2, 0, 0, 0, 0, 0, 0, 0},
        {4, 3, 0, 0, 0, 0, 0, 0, 0}, {4, 3, 2, 0, 0, 0, 0, 0, 0}, {4, 3, 3, 0, 0, 0, 0, 0, 0},
        {4, 3, 3, 1, 0, 0, 0, 0, 0}, {4, 3, 3, 2, 0, 0, 0, 0, 0}, {4, 3, 3, 3, 1, 0, 0, 0, 0},
        {4, 3, 3, 3, 2, 0, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0},
        {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 0},
        {4, 3, 3, 3, 2, 1, 1, 1, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 1}, {4, 3, 3, 3, 3, 1, 1, 1, 1},
        {4, 3, 3, 3, 3, 2, 1, 1, 1}, {4, 3, 3, 3, 3, 2, 2, 1, 1},
    };
    if(class_count > DND_MAX_CLASSES) class_count = DND_MAX_CLASSES;
    uint8_t caster_level = 0U;
    uint8_t shared_caster_count = 0U;
    const DndClassLevel* sole_shared_caster = NULL;
    for(uint8_t i = 0U; i < class_count; ++i) {
        const DndClassLevel* level = &classes[i];
        if(level->spellcasting_mode == DndSpellcastingFull ||
           level->spellcasting_mode == DndSpellcastingHalf ||
           level->spellcasting_mode == DndSpellcastingThird) {
            ++shared_caster_count;
            sole_shared_caster = level;
        }
    }
    if(shared_caster_count == 1U && sole_shared_caster &&
       sole_shared_caster->spellcasting_mode == DndSpellcastingThird) {
        caster_level = sole_shared_caster->level < 3U ? 0U : (sole_shared_caster->level + 2U) / 3U;
    } else {
        for(uint8_t i = 0U; i < class_count; ++i) {
            const DndClassLevel* level = &classes[i];
            if(level->spellcasting_mode == DndSpellcastingFull)
                caster_level += level->level;
            else if(level->spellcasting_mode == DndSpellcastingHalf)
                caster_level += (level->level + 1U) / 2U;
            else if(level->spellcasting_mode == DndSpellcastingThird)
                caster_level += level->level / 3U;
        }
    }
    if(caster_level > 20U) caster_level = 20U;
    spell_slots_max[0] = 0U;
    spell_slots_current[0] = 0U;
    for(uint8_t level = 1U; level <= 9U; ++level) {
        uint8_t maximum = caster_level ? slots[caster_level - 1U][level - 1U] : 0U;
        spell_slots_max[level] = maximum;
        if(spell_slots_current[level] > maximum) spell_slots_current[level] = maximum;
    }
}

void dndolphins_spells_recalculate_multiclass_slots(DndCharacter* character) {
    if(!character) return;
    dndolphins_spells_recalculate_shared_slots(
        character->classes,
        character->class_count,
        character->spell_slots_current,
        character->spell_slots_max);
}

bool dndolphins_spells_initialize_spell_slots_if_unset(DndCharacter* character) {
    if(!character) return false;
    bool changed = false;
    bool shared_unset = true;
    bool has_shared_caster = false;
    for(uint8_t level = 1U; level < DND_SLOT_COUNT; ++level) {
        if(character->spell_slots_current[level] || character->spell_slots_max[level]) {
            shared_unset = false;
            break;
        }
    }
    for(uint8_t index = 0U; index < character->class_count; ++index) {
        uint8_t mode = character->classes[index].spellcasting_mode;
        if(mode == DndSpellcastingFull || mode == DndSpellcastingHalf ||
           mode == DndSpellcastingThird) {
            has_shared_caster = true;
            break;
        }
    }
    if(shared_unset && has_shared_caster) {
        dndolphins_spells_recalculate_multiclass_slots(character);
        for(uint8_t level = 1U; level < DND_SLOT_COUNT; ++level) {
            character->spell_slots_current[level] = character->spell_slots_max[level];
            if(character->spell_slots_max[level]) changed = true;
        }
    }

    static const uint8_t pact_slots[20] = {1U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U,
                                           3U, 3U, 3U, 3U, 3U, 3U, 4U, 4U, 4U, 4U};
    static const uint8_t pact_levels[20] = {1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 5U,
                                            5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U};
    for(uint8_t index = 0U; index < character->class_count; ++index) {
        DndClassLevel* class_level = &character->classes[index];
        if(class_level->spellcasting_mode != DndSpellcastingPact) continue;
        if(class_level->pact_slot_level || class_level->pact_slots_current ||
           class_level->pact_slots_max)
            continue;
        uint8_t level = class_level->level;
        if(level < 1U) level = 1U;
        if(level > 20U) level = 20U;
        class_level->pact_slot_level = pact_levels[level - 1U];
        class_level->pact_slots_max = pact_slots[level - 1U];
        class_level->pact_slots_current = class_level->pact_slots_max;
        changed = true;
    }
    return changed;
}

static bool dndolphins_spells_class_name_is(const DndClassLevel* level, const char* name) {
    return level && name && strcmp(level->name, name) == 0;
}

static bool dndolphins_spells_subclass_name_is(const DndClassLevel* level, const char* name) {
    return level && name && strcmp(level->subclass, name) == 0;
}

static bool dndolphins_spells_is_eldritch_knight(const DndClassLevel* level) {
    return dndolphins_spells_class_name_is(level, "Fighter") &&
           dndolphins_spells_subclass_name_is(level, "Eldritch Knight");
}

static bool dndolphins_spells_is_arcane_trickster(const DndClassLevel* level) {
    return dndolphins_spells_class_name_is(level, "Rogue") &&
           dndolphins_spells_subclass_name_is(level, "Arcane Trickster");
}

bool dndolphins_spells_refresh_class_spellcasting(DndClassLevel* c) {
    if(!c) return false;
    uint8_t mode = c->spellcasting_mode;
    uint8_t ability = c->spellcasting_ability;
    bool recognized = true;

    if(dndolphins_spells_class_name_is(c, "Bard") ||
       dndolphins_spells_class_name_is(c, "Cleric") ||
       dndolphins_spells_class_name_is(c, "Druid") ||
       dndolphins_spells_class_name_is(c, "Sorcerer") ||
       dndolphins_spells_class_name_is(c, "Wizard")) {
        mode = DndSpellcastingFull;
    } else if(
        dndolphins_spells_class_name_is(c, "Artificer") ||
        dndolphins_spells_class_name_is(c, "Paladin") ||
        dndolphins_spells_class_name_is(c, "Ranger")) {
        mode = DndSpellcastingHalf;
    } else if(dndolphins_spells_class_name_is(c, "Warlock")) {
        mode = DndSpellcastingPact;
    } else if(dndolphins_spells_class_name_is(c, "Fighter")) {
        mode = dndolphins_spells_is_eldritch_knight(c) ? DndSpellcastingThird :
                                                         DndSpellcastingNone;
    } else if(dndolphins_spells_class_name_is(c, "Rogue")) {
        mode = dndolphins_spells_is_arcane_trickster(c) ? DndSpellcastingThird :
                                                          DndSpellcastingNone;
    } else if(
        dndolphins_spells_class_name_is(c, "Barbarian") ||
        dndolphins_spells_class_name_is(c, "Monk")) {
        mode = DndSpellcastingNone;
    } else {
        recognized = false;
    }

    if(dndolphins_spells_class_name_is(c, "Bard") ||
       dndolphins_spells_class_name_is(c, "Paladin") ||
       dndolphins_spells_class_name_is(c, "Sorcerer") ||
       dndolphins_spells_class_name_is(c, "Warlock"))
        ability = DndAbilityCharisma;
    else if(
        dndolphins_spells_class_name_is(c, "Cleric") ||
        dndolphins_spells_class_name_is(c, "Druid") ||
        dndolphins_spells_class_name_is(c, "Ranger"))
        ability = DndAbilityWisdom;
    else if(recognized)
        ability = DndAbilityIntelligence;

    bool changed = false;
    if(recognized && c->spellcasting_mode != mode) {
        c->spellcasting_mode = mode;
        changed = true;
    }
    if(recognized && c->spellcasting_ability != ability) {
        c->spellcasting_ability = ability;
        changed = true;
    }
    return changed;
}

static uint8_t dndolphins_spells_third_caster_prepared_limit(uint8_t level) {
    static const uint8_t prepared[18] = {
        3U, 4U, 4U, 4U, 5U, 6U, 6U, 7U, 8U, 8U, 9U, 10U, 10U, 11U, 11U, 11U, 12U, 13U};
    if(level < 3U) return 0U;
    if(level > 20U) level = 20U;
    return prepared[level - 3U];
}

bool dndolphins_spells_apply_level_progression(DndCharacter* character, uint8_t class_index) {
    if(!character || class_index >= character->class_count) return false;
    DndClassLevel* c = &character->classes[class_index];
    uint8_t level = c->level ? c->level : 1U;
    if(level > 20U) level = 20U;
    bool changed = false;
    bool shared_slots_were_unset = true;
    for(uint8_t spell_level = 1U; spell_level < DND_SLOT_COUNT; ++spell_level) {
        if(character->spell_slots_current[spell_level] ||
           character->spell_slots_max[spell_level]) {
            shared_slots_were_unset = false;
            break;
        }
    }

    if(c->hit_dice_max < level) {
        c->hit_dice_max = level;
        changed = true;
    }
    if(c->hit_dice_current > c->hit_dice_max) c->hit_dice_current = c->hit_dice_max;

    if(dndolphins_spells_refresh_class_spellcasting(c)) changed = true;
    uint8_t mode = c->spellcasting_mode;

    /* 2024/5.5e core spell-capacity tables are fixed by class level. Keeping
       these as O(1) table lookups avoids ability-score-dependent recalculation
       loops and makes the displayed knowable count match the class table. */
    static const uint8_t prepared_bard_cleric_druid[20] = {4U,  5U,  6U,  7U,  9U,  10U, 11U,
                                                           12U, 14U, 15U, 16U, 16U, 17U, 17U,
                                                           18U, 18U, 19U, 20U, 21U, 22U};
    static const uint8_t prepared_sorcerer[20] = {2U,  4U,  6U,  7U,  9U,  10U, 11U,
                                                  12U, 14U, 15U, 16U, 16U, 17U, 17U,
                                                  18U, 18U, 19U, 20U, 21U, 22U};
    static const uint8_t prepared_wizard[20] = {4U,  5U,  6U,  7U,  9U,  10U, 11U, 12U, 14U, 15U,
                                                16U, 16U, 17U, 18U, 19U, 21U, 22U, 23U, 24U, 25U};
    static const uint8_t prepared_half[20] = {2U,  3U,  4U,  5U,  6U,  6U,  7U,  7U,  9U,  9U,
                                              10U, 10U, 11U, 11U, 12U, 12U, 14U, 14U, 15U, 15U};
    static const uint8_t prepared_warlock[20] = {2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 10U,
                                                 11U, 11U, 12U, 12U, 13U, 13U, 14U, 14U, 15U, 15U};
    static const uint8_t cantrips_bard[20] = {2U, 2U, 2U, 3U, 3U, 3U, 3U, 3U, 3U, 4U,
                                              4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};
    static const uint8_t cantrips_cleric[20] = {3U, 3U, 3U, 4U, 4U, 4U, 4U, 4U, 4U, 5U,
                                                5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U};
    static const uint8_t cantrips_druid[20] = {2U, 2U, 2U, 3U, 3U, 3U, 3U, 3U, 3U, 4U,
                                               4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};
    static const uint8_t cantrips_sorcerer[20] = {4U, 4U, 4U, 5U, 5U, 5U, 5U, 5U, 5U, 6U,
                                                  6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U};
    static const uint8_t cantrips_wizard[20] = {3U, 3U, 3U, 4U, 4U, 4U, 4U, 4U, 4U, 5U,
                                                5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U};
    static const uint8_t cantrips_warlock[20] = {2U, 2U, 2U, 3U, 3U, 3U, 3U, 3U, 3U, 4U,
                                                 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};
    static const uint8_t cantrips_artificer[20] = {2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 3U,
                                                   3U, 3U, 3U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};

    uint8_t cantrips = 0U;
    uint8_t prepared = 0U;
    bool fixed_core_progression = true;
    if(dndolphins_spells_class_name_is(c, "Bard")) {
        cantrips = cantrips_bard[level - 1U];
        prepared = prepared_bard_cleric_druid[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Cleric")) {
        cantrips = cantrips_cleric[level - 1U];
        prepared = prepared_bard_cleric_druid[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Druid")) {
        cantrips = cantrips_druid[level - 1U];
        prepared = prepared_bard_cleric_druid[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Sorcerer")) {
        cantrips = cantrips_sorcerer[level - 1U];
        prepared = prepared_sorcerer[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Wizard")) {
        cantrips = cantrips_wizard[level - 1U];
        prepared = prepared_wizard[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Artificer")) {
        cantrips = cantrips_artificer[level - 1U];
        prepared = prepared_half[level - 1U];
    } else if(
        dndolphins_spells_class_name_is(c, "Paladin") ||
        dndolphins_spells_class_name_is(c, "Ranger")) {
        prepared = prepared_half[level - 1U];
    } else if(dndolphins_spells_class_name_is(c, "Warlock")) {
        cantrips = cantrips_warlock[level - 1U];
        prepared = prepared_warlock[level - 1U];
    } else {
        fixed_core_progression = false;
    }

    if(mode == DndSpellcastingThird) {
        cantrips = dndolphins_spells_is_arcane_trickster(c) ?
                       (level >= 10U ? 4U :
                        level >= 3U  ? 3U :
                                       0U) :
                       (dndolphins_spells_is_eldritch_knight(c) ? (level >= 10U ? 3U :
                                                                   level >= 3U  ? 2U :
                                                                                  0U) :
                                                                  0U);
        prepared = dndolphins_spells_third_caster_prepared_limit(level);
        if(c->cantrip_limit != cantrips) {
            c->cantrip_limit = cantrips;
            changed = true;
        }
        if(c->prepared_limit != prepared) {
            c->prepared_limit = prepared;
            changed = true;
        }
    } else if(fixed_core_progression) {
        if(c->cantrip_limit != cantrips) {
            c->cantrip_limit = cantrips;
            changed = true;
        }
        if(c->prepared_limit != prepared) {
            c->prepared_limit = prepared;
            changed = true;
        }
    } else if(mode == DndSpellcastingFull || mode == DndSpellcastingHalf) {
        /* Expansion/legacy classes without a verified fixed 2024 table retain
           the existing ability-modifier progression until their source data is
           explicitly verified. */
        int16_t ability_mod = c->spellcasting_ability < DND_ABILITY_COUNT ?
                                  dnd_rules_core_ability_modifier(
                                      character->ability_scores[c->spellcasting_ability]) :
                                  0;
        int16_t legacy_prepared = (int16_t)level + ability_mod;
        if(legacy_prepared < 1) legacy_prepared = 1;
        if(c->prepared_limit < (uint8_t)legacy_prepared) {
            c->prepared_limit = (uint8_t)legacy_prepared;
            changed = true;
        }
    } else if(
        mode == DndSpellcastingNone && (dndolphins_spells_class_name_is(c, "Fighter") ||
                                        dndolphins_spells_class_name_is(c, "Rogue"))) {
        if(c->cantrip_limit) {
            c->cantrip_limit = 0U;
            changed = true;
        }
        if(c->prepared_limit) {
            c->prepared_limit = 0U;
            changed = true;
        }
    }

    if(dndolphins_spells_class_name_is(c, "Wizard")) {
        uint16_t minimum = (uint16_t)(6U + (level > 1U ? 2U * (level - 1U) : 0U));
        if(c->spellbook_size < minimum) {
            c->spellbook_size = minimum;
            changed = true;
        }
    }
    if(dndolphins_spells_class_name_is(c, "Sorcerer")) {
        uint16_t points = level >= 2U ? level : 0U;
        if(c->spell_points_max < points) {
            c->spell_points_max = points;
            changed = true;
        }
        if(c->spell_points_current > c->spell_points_max)
            c->spell_points_current = c->spell_points_max;
    }
    if(dndolphins_spells_class_name_is(c, "Warlock")) {
        static const uint8_t pact_slots[20] = {1, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                               3, 3, 3, 3, 3, 3, 4, 4, 4, 4};
        static const uint8_t pact_level[20] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
                                               5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
        if(c->pact_slots_max != pact_slots[level - 1U]) {
            c->pact_slots_max = pact_slots[level - 1U];
            changed = true;
        }
        if(c->pact_slot_level != pact_level[level - 1U]) {
            c->pact_slot_level = pact_level[level - 1U];
            changed = true;
        }
        if(c->pact_slots_current > c->pact_slots_max) c->pact_slots_current = c->pact_slots_max;
        uint16_t mask = 0U;
        if(level >= 11U) mask |= (uint16_t)(1U << 6U);
        if(level >= 13U) mask |= (uint16_t)(1U << 7U);
        if(level >= 15U) mask |= (uint16_t)(1U << 8U);
        if(level >= 17U) mask |= (uint16_t)(1U << 9U);
        if((c->mystic_arcanum_mask & mask) != mask) {
            c->mystic_arcanum_mask |= mask;
            changed = true;
        }
    }
    dndolphins_spells_recalculate_multiclass_slots(character);
    if(shared_slots_were_unset && (c->spellcasting_mode == DndSpellcastingFull ||
                                   c->spellcasting_mode == DndSpellcastingHalf ||
                                   c->spellcasting_mode == DndSpellcastingThird)) {
        for(uint8_t spell_level = 1U; spell_level < DND_SLOT_COUNT; ++spell_level) {
            character->spell_slots_current[spell_level] = character->spell_slots_max[spell_level];
            if(character->spell_slots_max[spell_level]) changed = true;
        }
    }
    return changed;
}

uint8_t dndolphins_spells_point_cost(uint8_t level) {
    static const uint8_t cost[10] = {0U, 2U, 3U, 5U, 6U, 7U, 9U, 10U, 11U, 13U};
    return level < 10U ? cost[level] : 0U;
}

bool dndolphins_spells_is_tracked(const DndSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && (known || spell->prepared || always_prepared);
}

static bool
    dndolphins_spells_is_wizard_spell(const DndCharacter* character, const DndSpell* spell) {
    return character && spell && spell->class_index < character->class_count &&
           dndolphins_spells_class_name_is(&character->classes[spell->class_index], "Wizard");
}

static bool dndolphins_spells_normal_combat_cast_allowed(
    const DndCharacter* character,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared) {
    if(!character || !spell || !dndolphins_spells_is_tracked(spell, known, always_prepared))
        return false;
    if(spell->level == 0U) return true;
    if(!dndolphins_spells_is_wizard_spell(character, spell)) return true;
    /* Wizard level-1+ spells in combat must be prepared. Being present in the
       spellbook/known list alone is not enough. Free-cast availability is
       handled separately so an unprepared free-cast spell cannot also spend
       normal slots, Pact slots, spell points, or use the ritual option here. */
    return spell->prepared || always_prepared;
}

bool dndolphins_spells_can_ritual(const DndSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && spell->ritual && dndolphins_spells_is_tracked(spell, known, always_prepared);
}

bool dndolphins_spells_record_has_cast_resource(
    const DndCharacter* character,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current) {
    if(!character || !spell || !dndolphins_spells_is_tracked(spell, known, always_prepared))
        return false;
    if(spell->level == 0U || free_casts_current) return true;
    bool normal_cast =
        dndolphins_spells_normal_combat_cast_allowed(character, spell, known, always_prepared);
    if(!normal_cast) return false;
    for(uint8_t level = spell->level; level < DND_SLOT_COUNT; ++level)
        if(character->spell_slots_current[level]) return true;
    for(uint8_t class_index = 0U; class_index < character->class_count; ++class_index) {
        const DndClassLevel* class_level = &character->classes[class_index];
        if(class_level->spellcasting_mode == DndSpellcastingPact &&
           class_level->pact_slots_current && class_level->pact_slot_level >= spell->level)
            return true;
    }
    if(spell->class_index < character->class_count) {
        const DndClassLevel* class_level = &character->classes[spell->class_index];
        if(class_level->spellcasting_mode == DndSpellcastingSpellPoints) {
            uint8_t maximum = dnd_spell_eligibility_class_max_spell_level(class_level);
            if(maximum > 5U) maximum = 5U;
            for(uint8_t level = spell->level; level <= maximum; ++level) {
                uint8_t cost = dndolphins_spells_point_cost(level);
                if(cost && class_level->spell_points_current >= cost) return true;
            }
        }
    }
    /* Ritual casting is intentionally exposed through Combat -> Rituals, not
       through the normal Spell Attacks resource picker. */
    return false;
}

uint8_t dndolphins_spells_build_cast_options(
    const DndCharacter* character,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    DndSpellCastOption* options,
    uint8_t capacity) {
    if(!character || !spell || !dndolphins_spells_is_tracked(spell, known, always_prepared))
        return 0U;
    uint8_t count = 0U;
    bool normal_cast =
        dndolphins_spells_normal_combat_cast_allowed(character, spell, known, always_prepared);
#define DND_ADD_CAST_OPTION(lvl, kind, cls)     \
    do {                                        \
        if(options && count < capacity) {       \
            options[count].level = (lvl);       \
            options[count].resource = (kind);   \
            options[count].class_index = (cls); \
        }                                       \
        if(count < 255U) ++count;               \
    } while(false)

    if(spell->level == 0U) {
        DND_ADD_CAST_OPTION(0U, DndSpellCastCantrip, spell->class_index);
        return count;
    }
    if(free_casts_current) DND_ADD_CAST_OPTION(spell->level, DndSpellCastFree, spell->class_index);
    if(!normal_cast) return count;
    for(uint8_t level = spell->level; level < DND_SLOT_COUNT; ++level)
        if(character->spell_slots_current[level])
            DND_ADD_CAST_OPTION(level, DndSpellCastSlot, spell->class_index);
    for(uint8_t class_index = 0U; class_index < character->class_count; ++class_index) {
        const DndClassLevel* class_level = &character->classes[class_index];
        if(class_level->spellcasting_mode == DndSpellcastingPact &&
           class_level->pact_slots_current && class_level->pact_slot_level >= spell->level)
            DND_ADD_CAST_OPTION(class_level->pact_slot_level, DndSpellCastPact, class_index);
    }
    if(spell->class_index < character->class_count) {
        const DndClassLevel* class_level = &character->classes[spell->class_index];
        if(class_level->spellcasting_mode == DndSpellcastingSpellPoints) {
            uint8_t maximum = dnd_spell_eligibility_class_max_spell_level(class_level);
            if(maximum > 5U) maximum = 5U;
            for(uint8_t level = spell->level; level <= maximum; ++level) {
                uint8_t cost = dndolphins_spells_point_cost(level);
                if(cost && class_level->spell_points_current >= cost)
                    DND_ADD_CAST_OPTION(level, DndSpellCastPoints, spell->class_index);
            }
        }
    }
#undef DND_ADD_CAST_OPTION
    return count;
}

typedef struct {
    const DndCharacter* character;
    uint16_t* indices;
    uint16_t start;
    uint16_t capacity;
    uint16_t count;
} DndDolphinsCombatSpellIndexContext;

static bool dndolphins_spells_combat_spell_index_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)free_casts_max;
    DndDolphinsCombatSpellIndexContext* scan = context;
    if(!dndolphins_spells_record_has_cast_resource(
           scan->character, spell, known, always_prepared, free_casts_current))
        return true;
    uint8_t ability = dndolphins_spells_casting_ability_for(scan->character, spell);
    int8_t ability_modifier =
        dnd_rules_core_ability_modifier(scan->character->ability_scores[ability]);
    DndSpellDamageSpec damage;
    if(dndolphins_spell_combat_damage_spec(
           spell,
           spell->level,
           dnd_rules_core_total_level(scan->character),
           ability_modifier,
           &damage) &&
       (damage.resolution == DndSpellResolutionAttack ||
        damage.secondary_resolution == DndSpellResolutionAttack || damage.attack_rolls)) {
        if(scan->count >= scan->start && scan->count - scan->start < scan->capacity)
            scan->indices[scan->count - scan->start] = logical_index;
        ++scan->count;
    }
    return true;
}

bool dndolphins_spells_collect_combat_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count) {
    if(!storage || !character || !count || (capacity && !indices)) return false;
    DndDolphinsCombatSpellIndexContext context = {
        .character = character,
        .indices = indices,
        .start = start,
        .capacity = capacity,
        .count = 0U,
    };
    uint16_t total = 0U;
    bool success = dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_combat_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}

typedef DndDolphinsCombatSpellIndexContext DndDolphinsUtilitySpellIndexContext;

static bool dndolphins_spells_utility_spell_index_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)free_casts_max;
    DndDolphinsUtilitySpellIndexContext* scan = context;
    if(!dndolphins_spells_record_has_cast_resource(
           scan->character, spell, known, always_prepared, free_casts_current))
        return true;
    uint8_t ability = dndolphins_spells_casting_ability_for(scan->character, spell);
    int8_t ability_modifier =
        dnd_rules_core_ability_modifier(scan->character->ability_scores[ability]);
    DndSpellDamageSpec damage;
    bool mapped = dndolphins_spell_combat_damage_spec(
        spell,
        spell->level,
        dnd_rules_core_total_level(scan->character),
        ability_modifier,
        &damage);
    bool attack_roll = mapped && (damage.resolution == DndSpellResolutionAttack ||
                                  damage.secondary_resolution == DndSpellResolutionAttack ||
                                  damage.attack_rolls);
    if(attack_roll) return true;
    if(scan->count >= scan->start && scan->count - scan->start < scan->capacity)
        scan->indices[scan->count - scan->start] = logical_index;
    ++scan->count;
    return true;
}

bool dndolphins_spells_collect_utility_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count) {
    if(!storage || !character || !count || (capacity && !indices)) return false;
    DndDolphinsUtilitySpellIndexContext context = {
        .character = character,
        .indices = indices,
        .start = start,
        .capacity = capacity,
        .count = 0U,
    };
    uint16_t total = 0U;
    bool success = dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_utility_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}

typedef struct {
    const DndCharacter* character;
    uint16_t* indices;
    uint16_t start;
    uint16_t capacity;
    uint16_t count;
} DndDolphinsRitualSpellIndexContext;

static bool dndolphins_spells_ritual_spell_index_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)always_prepared;
    (void)free_casts_current;
    (void)free_casts_max;
    DndDolphinsRitualSpellIndexContext* scan = context;
    if(!spell || !known || !spell->ritual || spell->level == 0U ||
       !dndolphins_spells_is_wizard_spell(scan->character, spell))
        return true;
    if(scan->count >= scan->start && scan->count - scan->start < scan->capacity)
        scan->indices[scan->count - scan->start] = logical_index;
    ++scan->count;
    return true;
}

bool dndolphins_spells_collect_ritual_indices(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count) {
    if(!storage || !character || !count || (capacity && !indices)) return false;
    DndDolphinsRitualSpellIndexContext context = {
        .character = character,
        .indices = indices,
        .start = start,
        .capacity = capacity,
        .count = 0U,
    };
    uint16_t total = 0U;
    bool success = dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_ritual_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}
