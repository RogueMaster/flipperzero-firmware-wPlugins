#include "dndolphins_spells.h"

#include "dndolphins_rules.h"
#include "dndolphins_spell_combat.h"

#include <string.h>

uint8_t pocket_d20_spell_casting_ability_for(
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

int8_t pocket_d20_spell_attack_modifier_for(
    const PocketCharacter* character, const PocketSpell* spell) {
    uint8_t ability = pocket_d20_spell_casting_ability_for(character, spell);
    return (int8_t)(pocket_d20_ability_modifier(character->ability_scores[ability]) +
                    pocket_d20_proficiency_bonus(character) + character->spell_attack_misc +
                    pocket_d20_exhaustion_penalty(character));
}

int8_t pocket_d20_spell_save_dc_for(
    const PocketCharacter* character, const PocketSpell* spell) {
    uint8_t ability = pocket_d20_spell_casting_ability_for(character, spell);
    return (int8_t)(8 + pocket_d20_ability_modifier(character->ability_scores[ability]) +
                    pocket_d20_proficiency_bonus(character) + character->spell_save_misc);
}

int8_t pocket_d20_spell_attack_modifier(const PocketCharacter* character) {
    return pocket_d20_spell_attack_modifier_for(character, NULL);
}

int8_t pocket_d20_spell_save_dc(const PocketCharacter* character) {
    return pocket_d20_spell_save_dc_for(character, NULL);
}

void pocket_d20_recalculate_multiclass_slots(PocketCharacter* character) {
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
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        const PocketClassLevel* level = &character->classes[i];
        if(level->spellcasting_mode == PocketSpellcastingFull)
            caster_level += level->level;
        else if(level->spellcasting_mode == PocketSpellcastingHalf)
            caster_level += (level->level + 1U) / 2U;
        else if(level->spellcasting_mode == PocketSpellcastingThird)
            caster_level += level->level / 3U;
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

bool pocket_d20_initialize_spell_slots_if_unset(PocketCharacter* character) {
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
        pocket_d20_recalculate_multiclass_slots(character);
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

static bool pocket_d20_class_name_is(const PocketClassLevel* level, const char* name) {
    return level && name && strcmp(level->name, name) == 0;
}

bool pocket_d20_apply_level_progression(PocketCharacter* character, uint8_t class_index) {
    if(!character || class_index >= character->class_count) return false;
    PocketClassLevel* c = &character->classes[class_index];
    uint8_t level = c->level ? c->level : 1U;
    if(level > 20U) level = 20U;
    bool changed = false;

    if(c->hit_dice_max < level) { c->hit_dice_max = level; changed = true; }
    if(c->hit_dice_current > c->hit_dice_max) c->hit_dice_current = c->hit_dice_max;

    uint8_t mode = c->spellcasting_mode;
    if(pocket_d20_class_name_is(c, "Bard") || pocket_d20_class_name_is(c, "Cleric") ||
       pocket_d20_class_name_is(c, "Druid") || pocket_d20_class_name_is(c, "Sorcerer") ||
       pocket_d20_class_name_is(c, "Wizard")) mode = PocketSpellcastingFull;
    else if(pocket_d20_class_name_is(c, "Artificer") || pocket_d20_class_name_is(c, "Paladin") ||
            pocket_d20_class_name_is(c, "Ranger")) mode = PocketSpellcastingHalf;
    else if(pocket_d20_class_name_is(c, "Warlock")) mode = PocketSpellcastingPact;
    if(c->spellcasting_mode != mode) { c->spellcasting_mode = mode; changed = true; }

    uint8_t cantrips = 0U;
    if(pocket_d20_class_name_is(c, "Wizard") || pocket_d20_class_name_is(c, "Sorcerer")) cantrips = level >= 10U ? 5U : level >= 4U ? 4U : 3U;
    else if(pocket_d20_class_name_is(c, "Cleric") || pocket_d20_class_name_is(c, "Druid")) cantrips = level >= 10U ? 5U : level >= 4U ? 4U : 3U;
    else if(pocket_d20_class_name_is(c, "Bard")) cantrips = level >= 10U ? 4U : level >= 4U ? 3U : 2U;
    else if(pocket_d20_class_name_is(c, "Warlock")) cantrips = level >= 10U ? 4U : level >= 4U ? 3U : 2U;
    if(cantrips && c->cantrip_limit < cantrips) { c->cantrip_limit = cantrips; changed = true; }

    int16_t ability_mod = 0;
    if(c->spellcasting_ability < POCKET_D20_ABILITY_COUNT)
        ability_mod = pocket_d20_ability_modifier(character->ability_scores[c->spellcasting_ability]);
    int16_t prepared = (int16_t)level + ability_mod;
    if(prepared < 1) prepared = 1;
    if((mode == PocketSpellcastingFull || mode == PocketSpellcastingHalf) && c->prepared_limit < (uint8_t)prepared) {
        c->prepared_limit = (uint8_t)prepared; changed = true;
    }

    if(pocket_d20_class_name_is(c, "Wizard")) {
        uint16_t minimum = (uint16_t)(6U + (level > 1U ? 2U * (level - 1U) : 0U));
        if(c->spellbook_size < minimum) { c->spellbook_size = minimum; changed = true; }
    }
    if(pocket_d20_class_name_is(c, "Sorcerer")) {
        uint16_t points = level >= 2U ? level : 0U;
        if(c->spell_points_max < points) { c->spell_points_max = points; changed = true; }
        if(c->spell_points_current > c->spell_points_max) c->spell_points_current = c->spell_points_max;
    }
    if(pocket_d20_class_name_is(c, "Warlock")) {
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
    pocket_d20_recalculate_multiclass_slots(character);
    return changed;
}

uint8_t pocket_d20_class_max_spell_level(const PocketClassLevel* class_level) {
    if(!class_level) return 0U;
    uint8_t level = class_level->level;
    const char* name = class_level->name;
    if(!strcmp(name, "Bard") || !strcmp(name, "Cleric") || !strcmp(name, "Druid") ||
       !strcmp(name, "Sorcerer") || !strcmp(name, "Wizard")) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 9U ? 9U : maximum;
    }
    if(!strcmp(name, "Artificer") || !strcmp(name, "Paladin") || !strcmp(name, "Ranger")) {
        uint8_t maximum = (level + 3U) / 4U;
        return maximum > 5U ? 5U : maximum;
    }
    if(!strcmp(name, "Warlock")) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 5U ? 5U : maximum;
    }
    return 0U;
}

uint8_t pocket_d20_spell_point_cost(uint8_t level) {
    static const uint8_t cost[10] = {0U, 2U, 3U, 5U, 6U, 7U, 9U, 10U, 11U, 13U};
    return level < 10U ? cost[level] : 0U;
}

bool pocket_d20_spell_is_tracked(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && (known || spell->prepared || always_prepared);
}

bool pocket_d20_spell_can_ritual(
    const PocketSpell* spell, uint8_t known, uint8_t always_prepared) {
    return spell && spell->ritual && pocket_d20_spell_is_tracked(spell, known, always_prepared);
}

bool pocket_d20_spell_record_has_cast_resource(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current) {
    if(!character || !spell || !pocket_d20_spell_is_tracked(spell, known, always_prepared))
        return false;
    if(spell->level == 0U || free_casts_current) return true;
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
            uint8_t maximum = pocket_d20_class_max_spell_level(class_level);
            if(maximum > 5U) maximum = 5U;
            for(uint8_t level = spell->level; level <= maximum; ++level) {
                uint8_t cost = pocket_d20_spell_point_cost(level);
                if(cost && class_level->spell_points_current >= cost) return true;
            }
        }
    }
    return spell->ritual != 0U;
}

uint8_t pocket_d20_spells_build_cast_options(
    const PocketCharacter* character,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    PocketSpellCastOption* options,
    uint8_t capacity) {
    if(!character || !spell || !pocket_d20_spell_is_tracked(spell, known, always_prepared))
        return 0U;
    uint8_t count = 0U;
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
            uint8_t maximum = pocket_d20_class_max_spell_level(class_level);
            if(maximum > 5U) maximum = 5U;
            for(uint8_t level = spell->level; level <= maximum; ++level) {
                uint8_t cost = pocket_d20_spell_point_cost(level);
                if(cost && class_level->spell_points_current >= cost)
                    POCKET_ADD_CAST_OPTION(level, PocketSpellCastPoints, spell->class_index);
            }
        }
    }
    if(pocket_d20_spell_can_ritual(spell, known, always_prepared))
        POCKET_ADD_CAST_OPTION(spell->level, PocketSpellCastRitual, spell->class_index);
#undef POCKET_ADD_CAST_OPTION
    return count;
}

typedef struct {
    const PocketCharacter* character;
    uint8_t* indices;
    uint8_t capacity;
    uint8_t count;
} PocketD20CombatSpellIndexContext;

static bool pocket_d20_combat_spell_index_visitor(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)free_casts_max;
    PocketD20CombatSpellIndexContext* scan = context;
    if(!pocket_d20_spell_record_has_cast_resource(
           scan->character, spell, known, always_prepared, free_casts_current))
        return true;
    uint8_t ability = pocket_d20_spell_casting_ability_for(scan->character, spell);
    int8_t ability_modifier =
        pocket_d20_ability_modifier(scan->character->ability_scores[ability]);
    PocketSpellDamageSpec damage;
    if(pocket_d20_spell_damage_spec(
           spell,
           spell->level,
           pocket_d20_total_level(scan->character),
           ability_modifier,
           &damage) &&
       scan->count < scan->capacity)
        scan->indices[scan->count++] = logical_index;
    return true;
}

bool pocket_d20_spells_collect_combat_indices(
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
    bool success = pocket_d20_storage_visit_spells(
        storage, profile, pocket_d20_combat_spell_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}
