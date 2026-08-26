#include "pocket_d20_rules.h"

#include <furi_hal_random.h>

const char* const pocket_d20_ability_names[POCKET_D20_ABILITY_COUNT] = {
    "STR", "DEX", "CON", "INT", "WIS", "CHA"};

const char* const pocket_d20_skill_names[POCKET_D20_SKILL_COUNT] = {
    "Acrobatics",
    "Animal Handling",
    "Arcana",
    "Athletics",
    "Deception",
    "History",
    "Insight",
    "Intimidation",
    "Investigation",
    "Medicine",
    "Nature",
    "Perception",
    "Performance",
    "Persuasion",
    "Religion",
    "Sleight of Hand",
    "Stealth",
    "Survival",
};

const uint8_t pocket_d20_skill_abilities[POCKET_D20_SKILL_COUNT] = {
    PocketAbilityDexterity,
    PocketAbilityWisdom,
    PocketAbilityIntelligence,
    PocketAbilityStrength,
    PocketAbilityCharisma,
    PocketAbilityIntelligence,
    PocketAbilityWisdom,
    PocketAbilityCharisma,
    PocketAbilityIntelligence,
    PocketAbilityWisdom,
    PocketAbilityIntelligence,
    PocketAbilityWisdom,
    PocketAbilityCharisma,
    PocketAbilityCharisma,
    PocketAbilityIntelligence,
    PocketAbilityDexterity,
    PocketAbilityDexterity,
    PocketAbilityWisdom,
};

const char* const pocket_d20_damage_names[PocketDamageTypeCount] = {
    "Bludgeoning",
    "Piercing",
    "Slashing",
    "Acid",
    "Cold",
    "Fire",
    "Force",
    "Lightning",
    "Necrotic",
    "Poison",
    "Psychic",
    "Radiant",
    "Thunder",
};

const char* const pocket_d20_journal_category_names[PocketJournalCategoryCount] = {
    "Quick", "Adventure", "Item", "Milestone"};

static uint8_t pocket_d20_roll_one(uint8_t sides) {
    if(sides < 2U) return 0U;
    return (uint8_t)((furi_hal_random_get() % sides) + 1U);
}

int8_t pocket_d20_ability_modifier(int8_t score) {
    int16_t delta = (int16_t)score - 10;
    if(delta >= 0) return (int8_t)(delta / 2);
    return (int8_t)-(((-delta) + 1) / 2);
}

uint8_t pocket_d20_total_level(const PocketCharacter* character) {
    uint8_t level = 0U;
    for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i) {
        level += character->classes[i].level;
    }
    if(level < 1U) return 1U;
    if(level > 20U) return 20U;
    return level;
}

uint8_t pocket_d20_proficiency_bonus(const PocketCharacter* character) {
    return (uint8_t)(2U + ((pocket_d20_total_level(character) - 1U) / 4U));
}

static int8_t pocket_d20_apply_proficiency(
    int8_t base,
    uint8_t proficiency,
    uint8_t bonus) {
    if(proficiency == PocketProficiencyExpertise) return (int8_t)(base + (2 * bonus));
    if(proficiency == PocketProficiencyProficient) return (int8_t)(base + bonus);
    return base;
}

static int8_t pocket_d20_exhaustion_penalty(const PocketCharacter* character) {
    return (int8_t)(-2 * character->exhaustion);
}

int8_t pocket_d20_saving_throw_modifier(const PocketCharacter* character, uint8_t ability) {
    if(ability >= POCKET_D20_ABILITY_COUNT) return 0;
    int8_t base = pocket_d20_ability_modifier(character->ability_scores[ability]);
    return (int8_t)(pocket_d20_apply_proficiency(
                        base,
                        character->saving_throw_proficiency[ability],
                        pocket_d20_proficiency_bonus(character)) +
                    character->saving_throw_misc[ability] +
                    pocket_d20_exhaustion_penalty(character));
}

int8_t pocket_d20_skill_base_modifier(const PocketCharacter* character, uint8_t skill) {
    if(skill >= POCKET_D20_SKILL_COUNT) return 0;
    uint8_t ability = pocket_d20_skill_abilities[skill];
    int8_t base = pocket_d20_ability_modifier(character->ability_scores[ability]);
    return (int8_t)(pocket_d20_apply_proficiency(
                        base,
                        character->skill_proficiency[skill],
                        pocket_d20_proficiency_bonus(character)) +
                    character->skill_misc[skill]);
}

int8_t pocket_d20_skill_modifier(const PocketCharacter* character, uint8_t skill) {
    return (int8_t)(pocket_d20_skill_base_modifier(character, skill) +
                    pocket_d20_exhaustion_penalty(character));
}

int8_t pocket_d20_initiative_modifier(const PocketCharacter* character) {
    return (int8_t)(pocket_d20_ability_modifier(
                        character->ability_scores[PocketAbilityDexterity]) +
                    character->initiative_misc +
                    pocket_d20_exhaustion_penalty(character));
}

int16_t pocket_d20_effective_speed(const PocketCharacter* character) {
    int16_t speed = character->speed - (5 * character->exhaustion);
    return speed > 0 ? speed : 0;
}

int8_t pocket_d20_spell_attack_modifier(const PocketCharacter* character) {
    uint8_t ability = character->spellcasting_ability;
    if(ability >= POCKET_D20_ABILITY_COUNT) ability = PocketAbilityIntelligence;
    return (int8_t)(pocket_d20_ability_modifier(character->ability_scores[ability]) +
                    pocket_d20_proficiency_bonus(character) + character->spell_attack_misc +
                    pocket_d20_exhaustion_penalty(character));
}

int8_t pocket_d20_spell_save_dc(const PocketCharacter* character) {
    uint8_t ability = character->spellcasting_ability;
    if(ability >= POCKET_D20_ABILITY_COUNT) ability = PocketAbilityIntelligence;
    return (int8_t)(8 + pocket_d20_ability_modifier(character->ability_scores[ability]) +
                    pocket_d20_proficiency_bonus(character) + character->spell_save_misc);
}

uint8_t pocket_d20_class_prepared_count(const PocketCharacter* character, uint8_t class_index) {
    uint8_t count = 0U;
    for(uint8_t i = 0U; i < character->spell_count; ++i)
        if(character->spells[i].class_index == class_index &&
           (character->spells[i].prepared || character->spell_always_prepared[i]))
            ++count;
    return count;
}

uint8_t pocket_d20_class_known_count(const PocketCharacter* character, uint8_t class_index) {
    uint8_t count = 0U;
    for(uint8_t i = 0U; i < character->spell_count; ++i)
        if(character->spells[i].class_index == class_index && character->spell_known[i]) ++count;
    return count;
}

void pocket_d20_recalculate_multiclass_slots(PocketCharacter* character) {
    static const uint8_t slots[20][9] = {
        {2,0,0,0,0,0,0,0,0},{3,0,0,0,0,0,0,0,0},{4,2,0,0,0,0,0,0,0},
        {4,3,0,0,0,0,0,0,0},{4,3,2,0,0,0,0,0,0},{4,3,3,0,0,0,0,0,0},
        {4,3,3,1,0,0,0,0,0},{4,3,3,2,0,0,0,0,0},{4,3,3,3,1,0,0,0,0},
        {4,3,3,3,2,0,0,0,0},{4,3,3,3,2,1,0,0,0},{4,3,3,3,2,1,0,0,0},
        {4,3,3,3,2,1,1,0,0},{4,3,3,3,2,1,1,0,0},{4,3,3,3,2,1,1,1,0},
        {4,3,3,3,2,1,1,1,0},{4,3,3,3,2,1,1,1,1},{4,3,3,3,3,1,1,1,1},
        {4,3,3,3,3,2,1,1,1},{4,3,3,3,3,2,2,1,1},
    };
    uint8_t caster_level = 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        const PocketClassLevel* level = &character->classes[i];
        if(level->spellcasting_mode == PocketSpellcastingFull)
            caster_level += level->level;
        else if(level->spellcasting_mode == PocketSpellcastingHalf)
            caster_level += level->level / 2U;
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

int16_t pocket_d20_carried_weight_tenths(const PocketCharacter* character) {
    int32_t total = 0;
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        const PocketItem* item = &character->items[i];
        /* Containers organize inventory; their contents are still carried. */
        if(item->quantity > 0)
            total += (int32_t)item->weight_tenths * item->quantity;
    }
    return total > 32767 ? 32767 : (int16_t)total;
}

int16_t pocket_d20_equipped_weight_tenths(const PocketCharacter* character) {
    int32_t total = 0;
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        const PocketItem* item = &character->items[i];
        if(item->equipped && item->quantity > 0)
            total += (int32_t)item->weight_tenths * item->quantity;
    }
    return total > 32767 ? 32767 : (int16_t)total;
}

int16_t pocket_d20_carrying_capacity(const PocketCharacter* character) {
    if(character->carrying_capacity_override > 0) return character->carrying_capacity_override;
    return (int16_t)character->ability_scores[PocketAbilityStrength] * 15;
}

uint8_t pocket_d20_attuned_count(const PocketCharacter* character) {
    uint8_t count = 0U;
    for(uint8_t i = 0U; i < character->item_count; ++i)
        if(character->items[i].attuned) ++count;
    return count;
}

int16_t pocket_d20_calculated_armor_class(const PocketCharacter* character) {
    int16_t armor = 10;
    int16_t dexterity = pocket_d20_ability_modifier(
        character->ability_scores[PocketAbilityDexterity]);
    bool worn_armor = false;
    uint8_t shield = 0U;
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        const PocketItem* item = &character->items[i];
        if(!item->equipped) continue;
        if(item->shield_bonus) shield += item->shield_bonus;
        if(item->armor_base && !worn_armor) {
            int16_t dexterity_part = dexterity;
            if(item->armor_dex_cap >= 0 && dexterity_part > item->armor_dex_cap)
                dexterity_part = item->armor_dex_cap;
            armor = item->armor_base + dexterity_part;
            worn_armor = true;
        }
    }
    return armor + shield;
}

void pocket_d20_normalize_currency(PocketCharacter* character) {
    int64_t copper = character->currency_cp + character->currency_sp * 10LL +
                     character->currency_ep * 50LL + character->currency_gp * 100LL +
                     character->currency_pp * 1000LL;
    if(copper < 0) copper = 0;
    character->currency_pp = (int32_t)(copper / 1000LL);
    copper %= 1000LL;
    character->currency_gp = (int32_t)(copper / 100LL);
    copper %= 100LL;
    character->currency_ep = (int32_t)(copper / 50LL);
    copper %= 50LL;
    character->currency_sp = (int32_t)(copper / 10LL);
    character->currency_cp = (int32_t)(copper % 10LL);
}

int16_t pocket_d20_feature_max_uses(
    const PocketCharacter* character,
    const PocketFeature* feature) {
    if(feature->resource_formula == PocketResourceProficiency)
        return pocket_d20_proficiency_bonus(character);
    if(feature->resource_formula == PocketResourceAbility &&
       feature->resource_ability < POCKET_D20_ABILITY_COUNT) {
        int16_t modifier = pocket_d20_ability_modifier(
            character->ability_scores[feature->resource_ability]);
        return modifier > 0 ? modifier : 1;
    }
    return feature->uses_max;
}

int8_t pocket_d20_weapon_ability(const PocketCharacter* character, const PocketItem* item) {
    int8_t strength = pocket_d20_ability_modifier(
        character->ability_scores[PocketAbilityStrength]);
    int8_t dexterity = pocket_d20_ability_modifier(
        character->ability_scores[PocketAbilityDexterity]);
    switch(item->attack_ability) {
    case PocketAttackAbilityStrength:
        return strength;
    case PocketAttackAbilityDexterity:
        return dexterity;
    case PocketAttackAbilityBest:
        return strength > dexterity ? strength : dexterity;
    default:
        if(item->weapon_properties & PocketWeaponRanged) return dexterity;
        if(item->weapon_properties & PocketWeaponFinesse)
            return strength > dexterity ? strength : dexterity;
        return strength;
    }
}

int8_t pocket_d20_weapon_attack_modifier(
    const PocketCharacter* character,
    const PocketItem* item) {
    int8_t result = pocket_d20_weapon_ability(character, item) + item->magic_bonus;
    if(item->proficient) result += pocket_d20_proficiency_bonus(character);
    result += pocket_d20_exhaustion_penalty(character);
    return result;
}

uint16_t pocket_d20_roll_dice(uint8_t count, uint8_t sides) {
    return pocket_d20_roll_dice_values(count, sides, NULL, 0U);
}

uint16_t pocket_d20_roll_dice_values(
    uint8_t count,
    uint8_t sides,
    uint8_t* values,
    uint8_t capacity) {
    uint16_t total = 0;
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t roll = pocket_d20_roll_one(sides);
        total += roll;
        if(values && i < capacity) values[i] = roll;
    }
    return total;
}

PocketAttackRoll pocket_d20_roll_attack(
    const PocketCharacter* character,
    const PocketItem* item,
    PocketRollMode mode) {
    PocketAttackRoll result = {0};
    result.first_die = pocket_d20_roll_one(20U);
    result.natural_roll = result.first_die;
    if(mode == PocketRollAdvantage || mode == PocketRollDisadvantage) {
        result.second_die = pocket_d20_roll_one(20U);
        if(mode == PocketRollAdvantage) {
            if(result.second_die > result.natural_roll) result.natural_roll = result.second_die;
        } else if(result.second_die < result.natural_roll) {
            result.natural_roll = result.second_die;
        }
    }
    result.modifier = pocket_d20_weapon_attack_modifier(character, item);
    result.total = (int16_t)result.natural_roll + result.modifier;
    result.critical = result.natural_roll == 20U;
    result.automatic_miss = result.natural_roll == 1U;
    return result;
}

PocketDamageRoll pocket_d20_roll_damage(
    const PocketCharacter* character,
    const PocketItem* item,
    bool critical) {
    PocketDamageRoll result = {0};
    uint8_t multiplier = critical ? 2U : 1U;
    uint8_t die = item->damage_die;
    if(item->use_versatile && item->versatile_die >= 2U) die = item->versatile_die;
    result.weapon_roll_count = item->damage_dice * multiplier;
    result.extra_roll_count = item->extra_dice * multiplier;
    if(result.weapon_roll_count > POCKET_D20_MAX_DAMAGE_ROLLS)
        result.weapon_roll_count = POCKET_D20_MAX_DAMAGE_ROLLS;
    if(result.extra_roll_count > POCKET_D20_MAX_DAMAGE_ROLLS - result.weapon_roll_count)
        result.extra_roll_count = POCKET_D20_MAX_DAMAGE_ROLLS - result.weapon_roll_count;
    result.weapon_total = (int16_t)pocket_d20_roll_dice_values(
        result.weapon_roll_count,
        die,
        result.rolls,
        result.weapon_roll_count);
    result.extra_total = (int16_t)pocket_d20_roll_dice_values(
        result.extra_roll_count,
        item->extra_die,
        result.rolls + result.weapon_roll_count,
        result.extra_roll_count);
    result.modifier = item->magic_bonus;
    if(item->add_ability_damage) result.modifier += pocket_d20_weapon_ability(character, item);
    result.total = result.weapon_total + result.extra_total + result.modifier;
    if(result.total < 0) result.total = 0;
    result.critical = critical;
    return result;
}

void pocket_d20_short_rest(PocketCharacter* character) {
    for(uint8_t i = 0; i < character->feature_count; ++i) {
        if(character->features[i].recharge == PocketRechargeShortOrLong)
            character->features[i].uses_current =
                pocket_d20_feature_max_uses(character, &character->features[i]);
    }
}

int16_t pocket_d20_spend_hit_die(PocketCharacter* character, uint8_t* die_roll) {
    if(character->hp_current < 1 || character->hp_current >= character->hp_max ||
       character->hit_dice_current == 0U)
        return -1;
    uint8_t roll = pocket_d20_roll_one(character->hit_die);
    int16_t healing = roll + pocket_d20_ability_modifier(
                                 character->ability_scores[PocketAbilityConstitution]);
    if(healing < 1) healing = 1;
    int16_t missing = character->hp_max - character->hp_current;
    int16_t regained = healing < missing ? healing : missing;
    character->hp_current += regained;
    --character->hit_dice_current;
    if(die_roll) *die_roll = roll;
    return regained;
}

int16_t pocket_d20_spend_class_hit_die(
    PocketCharacter* character,
    uint8_t class_index,
    uint8_t* die_roll) {
    if(class_index >= character->class_count) return -1;
    PocketClassLevel* class_level = &character->classes[class_index];
    if(character->hp_current < 1 || character->hp_current >= character->hp_max ||
       class_level->hit_dice_current == 0U)
        return -1;
    uint8_t roll = pocket_d20_roll_one(class_level->hit_die);
    int16_t healing = roll + pocket_d20_ability_modifier(
                                 character->ability_scores[PocketAbilityConstitution]);
    if(healing < 1) healing = 1;
    int16_t missing = character->hp_max - character->hp_current;
    int16_t regained = healing < missing ? healing : missing;
    character->hp_current += regained;
    --class_level->hit_dice_current;
    if(die_roll) *die_roll = roll;
    return regained;
}

void pocket_d20_long_rest(PocketCharacter* character) {
    character->hp_current = character->hp_max;
    character->hp_temporary = 0;
    character->death_successes = 0;
    character->death_failures = 0;
    character->hit_dice_current = character->hit_dice_max;
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        character->classes[i].hit_dice_current = character->classes[i].hit_dice_max;
        character->classes[i].pact_slots_current = character->classes[i].pact_slots_max;
        character->classes[i].spell_points_current = character->classes[i].spell_points_max;
    }
    if(character->exhaustion) --character->exhaustion;
    character->arcane_recovery_used = 0U;
    for(uint8_t i = 1U; i < POCKET_D20_SLOT_COUNT; ++i) {
        character->spell_slots_current[i] = character->spell_slots_max[i];
    }
    for(uint8_t i = 0; i < character->feature_count; ++i)
        if(character->features[i].recharge != PocketRechargeManual &&
           character->features[i].recharge != PocketRechargeTurn &&
           character->features[i].recharge != PocketRechargeEncounter)
            character->features[i].uses_current =
                pocket_d20_feature_max_uses(character, &character->features[i]);
    for(uint8_t i = 0; i < character->spell_count; ++i) {
        character->spell_free_casts_current[i] = character->spell_free_casts_max[i];
    }
}
