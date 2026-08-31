#include "dndolphins_rules.h"

#include <furi_hal_random.h>

const char* const pocket_d20_ability_names[POCKET_D20_ABILITY_COUNT] =
    {"STR", "DEX", "CON", "INT", "WIS", "CHA"};

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

uint8_t pocket_d20_roll_die(uint8_t sides) {
    if(sides < 2U) return 0U;
    return (uint8_t)((furi_hal_random_get() % sides) + 1U);
}

int8_t pocket_d20_ability_modifier(int8_t score) {
    int16_t delta = (int16_t)score - 10;
    if(delta >= 0) return (int8_t)(delta / 2);
    return (int8_t) - (((-delta) + 1) / 2);
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

uint32_t pocket_d20_minimum_experience_for_level(uint8_t level) {
    static const uint32_t minimum_xp[20] = {
        0U,      300U,    900U,    2700U,   6500U,   14000U, 23000U,
        34000U,  48000U,  64000U,  85000U,  100000U, 120000U, 140000U,
        165000U, 195000U, 225000U, 265000U, 305000U, 355000U,
    };
    if(level < 1U) level = 1U;
    if(level > 20U) level = 20U;
    return minimum_xp[level - 1U];
}

void dndolphins_apply_experience_floor(PocketCharacter* character) {
    if(!character) return;
    uint32_t minimum = pocket_d20_minimum_experience_for_level(pocket_d20_total_level(character));
    if(character->experience < minimum) character->experience = minimum;
}

uint8_t pocket_d20_proficiency_bonus(const PocketCharacter* character) {
    uint8_t total_level = pocket_d20_total_level(character);
    if(total_level < 1U) total_level = 1U;
    return (uint8_t)(2U + ((total_level - 1U) / 4U));
}

static int8_t dndolphins_apply_proficiency(int8_t base, uint8_t proficiency, uint8_t bonus) {
    if(proficiency == PocketProficiencyExpertise) return (int8_t)(base + (2 * bonus));
    if(proficiency == PocketProficiencyProficient) return (int8_t)(base + bonus);
    return base;
}

int8_t pocket_d20_exhaustion_penalty(const PocketCharacter* character) {
    return (int8_t)(-2 * character->exhaustion);
}

int8_t pocket_d20_saving_throw_modifier(const PocketCharacter* character, uint8_t ability) {
    if(ability >= POCKET_D20_ABILITY_COUNT) return 0;
    int8_t base = pocket_d20_ability_modifier(character->ability_scores[ability]);
    return (int8_t)(dndolphins_apply_proficiency(
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
    return (int8_t)(dndolphins_apply_proficiency(
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
    return (
        int8_t)(pocket_d20_ability_modifier(character->ability_scores[PocketAbilityDexterity]) +
                character->initiative_misc + pocket_d20_exhaustion_penalty(character));
}

int16_t pocket_d20_effective_speed(const PocketCharacter* character) {
    int16_t speed = character->speed - (5 * character->exhaustion);
    return speed > 0 ? speed : 0;
}

int16_t
    pocket_d20_feature_max_uses(const PocketCharacter* character, const PocketFeature* feature) {
    if(feature->resource_formula == PocketResourceProficiency)
        return pocket_d20_proficiency_bonus(character);
    if(feature->resource_formula == PocketResourceAbility &&
       feature->resource_ability < POCKET_D20_ABILITY_COUNT) {
        int16_t modifier =
            pocket_d20_ability_modifier(character->ability_scores[feature->resource_ability]);
        return modifier > 0 ? modifier : 1;
    }
    return feature->uses_max;
}

uint16_t pocket_d20_roll_dice(uint8_t count, uint8_t sides) {
    return pocket_d20_roll_dice_values(count, sides, NULL, 0U);
}

uint8_t pocket_d20_roll_d20_mode(PocketRollMode mode) {
    uint8_t first = pocket_d20_roll_die(20U);
    if(mode != PocketRollAdvantage && mode != PocketRollDisadvantage) return first;
    uint8_t second = pocket_d20_roll_die(20U);
    if(mode == PocketRollAdvantage) return second > first ? second : first;
    return second < first ? second : first;
}

uint16_t
    pocket_d20_roll_dice_values(uint8_t count, uint8_t sides, uint8_t* values, uint8_t capacity) {
    uint16_t total = 0;
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t roll = pocket_d20_roll_die(sides);
        total += roll;
        if(values && i < capacity) values[i] = roll;
    }
    return total;
}

void pocket_d20_short_rest(PocketCharacter* character) {
    for(uint8_t i = 0; i < character->feature_count; ++i) {
        if(character->features[i].recharge == PocketRechargeShortOrLong)
            character->features[i].uses_current =
                pocket_d20_feature_max_uses(character, &character->features[i]);
    }
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
    uint8_t roll = pocket_d20_roll_die(class_level->hit_die);
    int16_t healing =
        roll + pocket_d20_ability_modifier(character->ability_scores[PocketAbilityConstitution]);
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
}
