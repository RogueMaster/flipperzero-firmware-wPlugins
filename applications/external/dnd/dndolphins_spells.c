#include "dndolphins_spells.h"

#include "dnd_rules.h"
#include "dndolphins_spell_combat.h"

#include <string.h>

typedef struct {
    DndDolphinsSpellClassCounts* counts;
} DndDolphinsSpellCountContext;

static bool dndolphins_spells_count_record(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)logical_index;
    (void)free_casts_current;
    (void)free_casts_max;
    DndDolphinsSpellCountContext* count_context = context;
    if(!count_context || !count_context->counts || !spell ||
       spell->class_index >= POCKET_D20_MAX_CLASSES)
        return true;
    uint8_t class_index = spell->class_index;
    if(known && count_context->counts->known[class_index] < UINT8_MAX)
        ++count_context->counts->known[class_index];
    if((spell->prepared || always_prepared) &&
       count_context->counts->prepared[class_index] < UINT8_MAX)
        ++count_context->counts->prepared[class_index];
    return true;
}

bool dndolphins_spells_class_counts(
    Storage* storage,
    uint32_t profile,
    DndDolphinsSpellClassCounts* counts,
    uint8_t* total_count) {
    if(!storage || !counts) return false;
    memset(counts, 0, sizeof(*counts));
    DndDolphinsSpellCountContext context = {.counts = counts};
    return dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_count_record, &context, total_count);
}

uint8_t dndolphins_spells_casting_ability_for(
    const PocketCharacter* character, const PocketSpell* spell) {
    if(spell) {
        uint8_t class_index = spell->class_index;
        if(class_index < character->class_count &&
           character->classes[class_index].spellcasting_mode != PocketSpellcastingNone &&
           character->classes[class_index].spellcasting_ability < POCKET_D20_ABILITY_COUNT)
            return character->classes[class_index].spellcasting_ability;
    }
    return character->spellcasting_ability < POCKET_D20_ABILITY_COUNT ?
               character->spellcasting_ability :
               PocketAbilityIntelligence;
}

int8_t dndolphins_spells_attack_modifier_for(
    const PocketCharacter* character, const PocketSpell* spell) {
    uint8_t ability = dndolphins_spells_casting_ability_for(character, spell);
    return (int8_t)(dnd_rules_core_ability_modifier(character->ability_scores[ability]) +
                    dnd_rules_core_proficiency_bonus(character) + character->spell_attack_misc +
                    dnd_rules_core_exhaustion_penalty(character));
}

int8_t dndolphins_spells_save_dc_for(
    const PocketCharacter* character, const PocketSpell* spell) {
    uint8_t ability = dndolphins_spells_casting_ability_for(character, spell);
    return (int8_t)(8 + dnd_rules_core_ability_modifier(character->ability_scores[ability]) +
                    dnd_rules_core_proficiency_bonus(character) + character->spell_save_misc);
}

int8_t dndolphins_spells_attack_modifier(const PocketCharacter* character) {
    return dndolphins_spells_attack_modifier_for(character, NULL);
}

int8_t dndolphins_spells_save_dc(const PocketCharacter* character) {
    return dndolphins_spells_save_dc_for(character, NULL);
}

void dndolphins_spells_recalculate_multiclass_slots(PocketCharacter* character) {
    static const uint8_t slots[20][9] = {
        {2, 0, 0, 0, 0, 0, 0, 0, 0}, {3, 0, 0, 0, 0, 0, 0, 0, 0}, {4, 2, 0, 0, 0, 0, 0, 0, 0},
        {4, 3, 0, 0, 0, 0, 0, 0, 0}, {4, 3, 2, 0, 0, 0, 0, 0, 0}, {4, 3, 3, 0, 0, 0, 0, 0, 0},
        {4, 3, 3, 1, 0, 0, 0, 0, 0}, {4, 3, 3, 2, 0, 0, 0, 0, 0}, {4, 3, 3, 3, 1, 0, 0, 0, 0},
        {4, 3, 3, 3, 2, 0, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0}, {4, 3, 3, 3, 2, 1, 0, 0, 0},
        {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 0, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 0},
        {4, 3, 3, 3, 2, 1, 1, 1, 0}, {4, 3, 3, 3, 2, 1, 1, 1, 1}, {4, 3, 3, 3, 3, 1, 1, 1, 1},
        {4, 3, 3, 3, 3, 2, 1, 1, 1}, {4, 3, 3, 3, 3, 2, 2, 1, 1},
    };
    uint8_t caster_level = 0U;
    uint8_t shared_caster_count = 0U;
    const PocketClassLevel* sole_shared_caster = NULL;
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        const PocketClassLevel* level = &character->classes[i];
        if(level->spellcasting_mode == PocketSpellcastingFull ||
           level->spellcasting_mode == PocketSpellcastingHalf ||
           level->spellcasting_mode == PocketSpellcastingThird) {
            ++shared_caster_count;
            sole_shared_caster = level;
        }
    }

    /* If only one class supplies the Spellcasting feature, use that class's
       native progression. This only differs from the shared multiclass table
       for third casters: Eldritch Knight/Arcane Trickster use ceil(level/3)
       for their own subclass slot table, but contribute floor(level/3) when
       combined with another Spellcasting class. Pact Magic is separate and
       does not turn a sole third caster into a multiclass Spellcasting pool. */
    if(shared_caster_count == 1U && sole_shared_caster &&
       sole_shared_caster->spellcasting_mode == PocketSpellcastingThird) {
        caster_level = sole_shared_caster->level < 3U ?
                           0U :
                           (sole_shared_caster->level + 2U) / 3U;
    } else {
        for(uint8_t i = 0U; i < character->class_count; ++i) {
            const PocketClassLevel* level = &character->classes[i];
            if(level->spellcasting_mode == PocketSpellcastingFull)
                caster_level += level->level;
            else if(level->spellcasting_mode == PocketSpellcastingHalf)
                caster_level += (level->level + 1U) / 2U;
            else if(level->spellcasting_mode == PocketSpellcastingThird)
                caster_level += level->level / 3U;
        }
    }
    if(caster_level > 20U) caster_level = 20U;
    character->spell_slots_max[0] = 0U;
    character->spell_slots_current[0] = 0U;
    for(uint8_t level = 1U; level <= 9U; ++level) {
        uint8_t maximum = caster_level ? slots[caster_level - 1U][level - 1U] : 0U;
        character->spell_slots_max[level] = maximum;
        if(character->spell_slots_current[level] > maximum)
            character->spell_slots_current[level] = maximum;
    }
}

bool dndolphins_spells_initialize_spell_slots_if_unset(PocketCharacter* character) {
    if(!character) return false;
    bool changed = false;
    bool shared_unset = true;
    bool has_shared_caster = false;
    for(uint8_t level = 1U; level < POCKET_D20_SLOT_COUNT; ++level) {
        if(character->spell_slots_current[level] || character->spell_slots_max[level]) {
            shared_unset = false;
            break;
        }
    }
    for(uint8_t index = 0U; index < character->class_count; ++index) {
        uint8_t mode = character->classes[index].spellcasting_mode;
        if(mode == PocketSpellcastingFull || mode == PocketSpellcastingHalf ||
           mode == PocketSpellcastingThird) {
            has_shared_caster = true;
            break;
        }
    }
    if(shared_unset && has_shared_caster) {
        dndolphins_spells_recalculate_multiclass_slots(character);
        for(uint8_t level = 1U; level < POCKET_D20_SLOT_COUNT; ++level) {
            character->spell_slots_current[level] = character->spell_slots_max[level];
            if(character->spell_slots_max[level]) changed = true;
        }
    }

    static const uint8_t pact_slots[20] = {
        1U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U,
        3U, 3U, 3U, 3U, 3U, 3U, 4U, 4U, 4U, 4U};
    static const uint8_t pact_levels[20] = {
        1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 5U,
        5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U};
    for(uint8_t index = 0U; index < character->class_count; ++index) {
        PocketClassLevel* class_level = &character->classes[index];
        if(class_level->spellcasting_mode != PocketSpellcastingPact) continue;
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

static bool dndolphins_spells_class_name_is(const PocketClassLevel* level, const char* name) {
    return level && name && strcmp(level->name, name) == 0;
}

static bool dndolphins_spells_subclass_name_is(const PocketClassLevel* level, const char* name) {
    return level && name && strcmp(level->subclass, name) == 0;
}

static bool dndolphins_spells_is_eldritch_knight(const PocketClassLevel* level) {
    return dndolphins_spells_class_name_is(level, "Fighter") &&
           dndolphins_spells_subclass_name_is(level, "Eldritch Knight");
}

static bool dndolphins_spells_is_arcane_trickster(const PocketClassLevel* level) {
    return dndolphins_spells_class_name_is(level, "Rogue") &&
           dndolphins_spells_subclass_name_is(level, "Arcane Trickster");
}

bool dndolphins_spells_refresh_class_spellcasting(PocketClassLevel* c) {
    if(!c) return false;
    uint8_t mode = c->spellcasting_mode;
    uint8_t ability = c->spellcasting_ability;
    bool recognized = true;

    if(dndolphins_spells_class_name_is(c, "Bard") || dndolphins_spells_class_name_is(c, "Cleric") ||
       dndolphins_spells_class_name_is(c, "Druid") || dndolphins_spells_class_name_is(c, "Sorcerer") ||
       dndolphins_spells_class_name_is(c, "Wizard")) {
        mode = PocketSpellcastingFull;
    } else if(dndolphins_spells_class_name_is(c, "Artificer") || dndolphins_spells_class_name_is(c, "Paladin") ||
              dndolphins_spells_class_name_is(c, "Ranger")) {
        mode = PocketSpellcastingHalf;
    } else if(dndolphins_spells_class_name_is(c, "Warlock")) {
        mode = PocketSpellcastingPact;
    } else if(dndolphins_spells_class_name_is(c, "Fighter")) {
        mode = dndolphins_spells_is_eldritch_knight(c) ? PocketSpellcastingThird : PocketSpellcastingNone;
    } else if(dndolphins_spells_class_name_is(c, "Rogue")) {
        mode = dndolphins_spells_is_arcane_trickster(c) ? PocketSpellcastingThird : PocketSpellcastingNone;
    } else if(dndolphins_spells_class_name_is(c, "Barbarian") || dndolphins_spells_class_name_is(c, "Monk")) {
        mode = PocketSpellcastingNone;
    } else {
        recognized = false;
    }

    if(dndolphins_spells_class_name_is(c, "Bard") || dndolphins_spells_class_name_is(c, "Paladin") ||
       dndolphins_spells_class_name_is(c, "Sorcerer") || dndolphins_spells_class_name_is(c, "Warlock"))
        ability = PocketAbilityCharisma;
    else if(dndolphins_spells_class_name_is(c, "Cleric") || dndolphins_spells_class_name_is(c, "Druid") ||
            dndolphins_spells_class_name_is(c, "Ranger"))
        ability = PocketAbilityWisdom;
    else if(recognized)
        ability = PocketAbilityIntelligence;

    bool changed = false;
    if(recognized && c->spellcasting_mode != mode) { c->spellcasting_mode = mode; changed = true; }
    if(recognized && c->spellcasting_ability != ability) {
        c->spellcasting_ability = ability;
        changed = true;
    }
    return changed;
}

static uint8_t dndolphins_spells_third_caster_prepared_limit(uint8_t level) {
    static const uint8_t prepared[18] = {
        3U, 4U, 4U, 4U, 5U, 6U, 6U, 7U, 8U,
        8U, 9U, 10U, 10U, 11U, 11U, 11U, 12U, 13U};
    if(level < 3U) return 0U;
    if(level > 20U) level = 20U;
    return prepared[level - 3U];
}

bool dndolphins_spells_apply_level_progression(PocketCharacter* character, uint8_t class_index) {
    if(!character || class_index >= character->class_count) return false;
    PocketClassLevel* c = &character->classes[class_index];
    uint8_t level = c->level ? c->level : 1U;
    if(level > 20U) level = 20U;
    bool changed = false;
    bool shared_slots_were_unset = true;
    for(uint8_t spell_level = 1U; spell_level < POCKET_D20_SLOT_COUNT; ++spell_level) {
        if(character->spell_slots_current[spell_level] || character->spell_slots_max[spell_level]) {
            shared_slots_were_unset = false;
            break;
        }
    }

    if(c->hit_dice_max < level) { c->hit_dice_max = level; changed = true; }
    if(c->hit_dice_current > c->hit_dice_max) c->hit_dice_current = c->hit_dice_max;

    if(dndolphins_spells_refresh_class_spellcasting(c)) changed = true;
    uint8_t mode = c->spellcasting_mode;

    uint8_t cantrips = 0U;
    if(dndolphins_spells_class_name_is(c, "Wizard") || dndolphins_spells_class_name_is(c, "Sorcerer")) cantrips = level >= 10U ? 5U : level >= 4U ? 4U : 3U;
    else if(dndolphins_spells_class_name_is(c, "Cleric") || dndolphins_spells_class_name_is(c, "Druid")) cantrips = level >= 10U ? 5U : level >= 4U ? 4U : 3U;
    else if(dndolphins_spells_class_name_is(c, "Bard")) cantrips = level >= 10U ? 4U : level >= 4U ? 3U : 2U;
    else if(dndolphins_spells_class_name_is(c, "Warlock")) cantrips = level >= 10U ? 4U : level >= 4U ? 3U : 2U;
    else if(dndolphins_spells_is_eldritch_knight(c)) cantrips = level >= 10U ? 3U : level >= 3U ? 2U : 0U;
    else if(dndolphins_spells_is_arcane_trickster(c)) cantrips = level >= 10U ? 4U : level >= 3U ? 3U : 0U;
    if(mode == PocketSpellcastingThird) {
        if(c->cantrip_limit != cantrips) { c->cantrip_limit = cantrips; changed = true; }
    } else if(cantrips && c->cantrip_limit < cantrips) {
        c->cantrip_limit = cantrips;
        changed = true;
    } else if(mode == PocketSpellcastingNone &&
              (dndolphins_spells_class_name_is(c, "Fighter") || dndolphins_spells_class_name_is(c, "Rogue")) &&
              c->cantrip_limit) {
        c->cantrip_limit = 0U;
        changed = true;
    }

    int16_t ability_mod = 0;
    if(c->spellcasting_ability < POCKET_D20_ABILITY_COUNT)
        ability_mod = dnd_rules_core_ability_modifier(character->ability_scores[c->spellcasting_ability]);
    int16_t prepared = (int16_t)level + ability_mod;
    if(prepared < 1) prepared = 1;
    if((mode == PocketSpellcastingFull || mode == PocketSpellcastingHalf) && c->prepared_limit < (uint8_t)prepared) {
        c->prepared_limit = (uint8_t)prepared; changed = true;
    } else if(mode == PocketSpellcastingThird) {
        uint8_t third_prepared = dndolphins_spells_third_caster_prepared_limit(level);
        if(c->prepared_limit != third_prepared) {
            c->prepared_limit = third_prepared;
            changed = true;
        }
    } else if(mode == PocketSpellcastingNone &&
              (dndolphins_spells_class_name_is(c, "Fighter") || dndolphins_spells_class_name_is(c, "Rogue")) &&
              c->prepared_limit) {
        c->prepared_limit = 0U;
        changed = true;
    }

    if(dndolphins_spells_class_name_is(c, "Wizard")) {
        uint16_t minimum = (uint16_t)(6U + (level > 1U ? 2U * (level - 1U) : 0U));
        if(c->spellbook_size < minimum) { c->spellbook_size = minimum; changed = true; }
    }
    if(dndolphins_spells_class_name_is(c, "Sorcerer")) {
        uint16_t points = level >= 2U ? level : 0U;
        if(c->spell_points_max < points) { c->spell_points_max = points; changed = true; }
        if(c->spell_points_current > c->spell_points_max) c->spell_points_current = c->spell_points_max;
    }
    if(dndolphins_spells_class_name_is(c, "Warlock")) {
        static const uint8_t pact_slots[20] = {1,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,4};
        static const uint8_t pact_level[20] = {1,1,2,2,3,3,4,4,5,5,5,5,5,5,5,5,5,5,5,5};
        if(c->pact_slots_max != pact_slots[level-1U]) { c->pact_slots_max = pact_slots[level-1U]; changed = true; }
        if(c->pact_slot_level != pact_level[level-1U]) { c->pact_slot_level = pact_level[level-1U]; changed = true; }
        if(c->pact_slots_current > c->pact_slots_max) c->pact_slots_current = c->pact_slots_max;
        uint16_t mask = 0U;
        if(level >= 11U) mask |= (uint16_t)(1U << 6U);
        if(level >= 13U) mask |= (uint16_t)(1U << 7U);
        if(level >= 15U) mask |= (uint16_t)(1U << 8U);
        if(level >= 17U) mask |= (uint16_t)(1U << 9U);
        if((c->mystic_arcanum_mask & mask) != mask) { c->mystic_arcanum_mask |= mask; changed = true; }
    }
    dndolphins_spells_recalculate_multiclass_slots(character);
    if(shared_slots_were_unset &&
       (c->spellcasting_mode == PocketSpellcastingFull ||
        c->spellcasting_mode == PocketSpellcastingHalf ||
        c->spellcasting_mode == PocketSpellcastingThird)) {
        for(uint8_t spell_level = 1U; spell_level < POCKET_D20_SLOT_COUNT; ++spell_level) {
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

bool dndolphins_spells_is_tracked(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && (known || spell->prepared || always_prepared);
}

static bool dndolphins_spells_is_wizard_spell(
    const PocketCharacter* character, const PocketSpell* spell) {
    return character && spell && spell->class_index < character->class_count &&
           dndolphins_spells_class_name_is(&character->classes[spell->class_index], "Wizard");
}

static bool dndolphins_spells_normal_combat_cast_allowed(
    const PocketCharacter* character,
    const PocketSpell* spell,
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

bool dndolphins_spells_can_ritual(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && spell->ritual && dndolphins_spells_is_tracked(spell, known, always_prepared);
}

bool dndolphins_spells_record_has_cast_resource(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current) {
    if(!character || !spell || !dndolphins_spells_is_tracked(spell, known, always_prepared))
        return false;
    if(spell->level == 0U || free_casts_current) return true;
    bool normal_cast = dndolphins_spells_normal_combat_cast_allowed(
        character, spell, known, always_prepared);
    if(!normal_cast) return false;
    for(uint8_t level = spell->level; level < POCKET_D20_SLOT_COUNT; ++level)
        if(character->spell_slots_current[level]) return true;
    for(uint8_t class_index = 0U; class_index < character->class_count; ++class_index) {
        const PocketClassLevel* class_level = &character->classes[class_index];
        if(class_level->spellcasting_mode == PocketSpellcastingPact &&
           class_level->pact_slots_current && class_level->pact_slot_level >= spell->level)
            return true;
    }
    if(spell->class_index < character->class_count) {
        const PocketClassLevel* class_level = &character->classes[spell->class_index];
        if(class_level->spellcasting_mode == PocketSpellcastingSpellPoints) {
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
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    PocketSpellCastOption* options,
    uint8_t capacity) {
    if(!character || !spell || !dndolphins_spells_is_tracked(spell, known, always_prepared))
        return 0U;
    uint8_t count = 0U;
    bool normal_cast = dndolphins_spells_normal_combat_cast_allowed(
        character, spell, known, always_prepared);
#define POCKET_ADD_CAST_OPTION(lvl, kind, cls)                                                    \
    do {                                                                                          \
        if(options && count < capacity) {                                                         \
            options[count].level = (lvl);                                                         \
            options[count].resource = (kind);                                                     \
            options[count].class_index = (cls);                                                   \
        }                                                                                         \
        if(count < 255U) ++count;                                                                 \
    } while(false)

    if(spell->level == 0U) {
        POCKET_ADD_CAST_OPTION(0U, PocketSpellCastCantrip, spell->class_index);
        return count;
    }
    if(free_casts_current)
        POCKET_ADD_CAST_OPTION(spell->level, PocketSpellCastFree, spell->class_index);
    if(!normal_cast) return count;
    for(uint8_t level = spell->level; level < POCKET_D20_SLOT_COUNT; ++level)
        if(character->spell_slots_current[level])
            POCKET_ADD_CAST_OPTION(level, PocketSpellCastSlot, spell->class_index);
    for(uint8_t class_index = 0U; class_index < character->class_count; ++class_index) {
        const PocketClassLevel* class_level = &character->classes[class_index];
        if(class_level->spellcasting_mode == PocketSpellcastingPact &&
           class_level->pact_slots_current && class_level->pact_slot_level >= spell->level)
            POCKET_ADD_CAST_OPTION(class_level->pact_slot_level, PocketSpellCastPact, class_index);
    }
    if(spell->class_index < character->class_count) {
        const PocketClassLevel* class_level = &character->classes[spell->class_index];
        if(class_level->spellcasting_mode == PocketSpellcastingSpellPoints) {
            uint8_t maximum = dnd_spell_eligibility_class_max_spell_level(class_level);
            if(maximum > 5U) maximum = 5U;
            for(uint8_t level = spell->level; level <= maximum; ++level) {
                uint8_t cost = dndolphins_spells_point_cost(level);
                if(cost && class_level->spell_points_current >= cost)
                    POCKET_ADD_CAST_OPTION(level, PocketSpellCastPoints, spell->class_index);
            }
        }
    }
#undef POCKET_ADD_CAST_OPTION
    return count;
}

typedef struct {
    const PocketCharacter* character;
    uint8_t* indices;
    uint8_t capacity;
    uint8_t count;
} PocketD20CombatSpellIndexContext;

static bool dndolphins_spells_combat_spell_index_visitor(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)free_casts_max;
    PocketD20CombatSpellIndexContext* scan = context;
    if(!dndolphins_spells_record_has_cast_resource(
           scan->character, spell, known, always_prepared, free_casts_current))
        return true;
    uint8_t ability = dndolphins_spells_casting_ability_for(scan->character, spell);
    int8_t ability_modifier =
        dnd_rules_core_ability_modifier(scan->character->ability_scores[ability]);
    PocketSpellDamageSpec damage;
    if(dndolphins_spell_combat_damage_spec(
           spell,
           spell->level,
           dnd_rules_core_total_level(scan->character),
           ability_modifier,
           &damage) &&
       scan->count < scan->capacity)
        scan->indices[scan->count++] = logical_index;
    return true;
}

bool dndolphins_spells_collect_combat_indices(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count) {
    if(!storage || !character || !indices || !count) return false;
    PocketD20CombatSpellIndexContext context = {
        .character = character,
        .indices = indices,
        .capacity = capacity,
        .count = 0U,
    };
    uint8_t total = 0U;
    bool success = dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_combat_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}

typedef struct {
    const PocketCharacter* character;
    uint8_t* indices;
    uint8_t capacity;
    uint8_t count;
} PocketD20RitualSpellIndexContext;

static bool dndolphins_spells_ritual_spell_index_visitor(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)always_prepared;
    (void)free_casts_current;
    (void)free_casts_max;
    PocketD20RitualSpellIndexContext* scan = context;
    if(!spell || !known || !spell->ritual || spell->level == 0U ||
       !dndolphins_spells_is_wizard_spell(scan->character, spell))
        return true;
    if(scan->count < scan->capacity) scan->indices[scan->count++] = logical_index;
    return true;
}

bool dndolphins_spells_collect_ritual_indices(
    Storage* storage,
    uint32_t profile,
    const PocketCharacter* character,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count) {
    if(!storage || !character || !indices || !count) return false;
    PocketD20RitualSpellIndexContext context = {
        .character = character,
        .indices = indices,
        .capacity = capacity,
        .count = 0U,
    };
    uint8_t total = 0U;
    bool success = dnd_storage_visit_spells(
        storage, profile, dndolphins_spells_ritual_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}
