#include "dnd_rules.h"

#include <furi_hal_random.h>

const char* const dnd_rules_core_ability_names[POCKET_D20_ABILITY_COUNT] =
    {"STR", "DEX", "CON", "INT", "WIS", "CHA"};

const char* const dnd_rules_core_skill_names[POCKET_D20_SKILL_COUNT] = {
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

const uint8_t dnd_rules_core_skill_abilities[POCKET_D20_SKILL_COUNT] = {
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

const char* const dnd_rules_core_damage_names[PocketDamageTypeCount] = {
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

uint8_t dnd_rules_core_roll_die(uint8_t sides) {
    if(sides < 2U) return 0U;
    return (uint8_t)((furi_hal_random_get() % sides) + 1U);
}

int8_t dnd_rules_core_ability_modifier(int8_t score) {
    int16_t delta = (int16_t)score - 10;
    if(delta >= 0) return (int8_t)(delta / 2);
    return (int8_t)-(((-delta) + 1) / 2);
}

uint8_t dnd_rules_core_total_level(const PocketCharacter* character) {
    uint8_t level = 0U;
    for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        level += character->classes[i].level;
    if(level < 1U) return 1U;
    if(level > 20U) return 20U;
    return level;
}

uint8_t dnd_rules_core_proficiency_bonus(const PocketCharacter* character) {
    uint8_t total_level = dnd_rules_core_total_level(character);
    if(total_level < 1U) total_level = 1U;
    return (uint8_t)(2U + ((total_level - 1U) / 4U));
}

static int8_t dnd_rules_core_apply_proficiency(int8_t base, uint8_t proficiency, uint8_t bonus) {
    if(proficiency == PocketProficiencyExpertise) return (int8_t)(base + (2 * bonus));
    if(proficiency == PocketProficiencyProficient) return (int8_t)(base + bonus);
    return base;
}

int8_t dnd_rules_core_exhaustion_penalty(const PocketCharacter* character) {
    return (int8_t)(-2 * character->exhaustion);
}

int8_t dnd_rules_core_saving_throw_modifier(const PocketCharacter* character, uint8_t ability) {
    if(ability >= POCKET_D20_ABILITY_COUNT) return 0;
    int8_t base = dnd_rules_core_ability_modifier(character->ability_scores[ability]);
    return (int8_t)(dnd_rules_core_apply_proficiency(
                        base,
                        character->saving_throw_proficiency[ability],
                        dnd_rules_core_proficiency_bonus(character)) +
                    character->saving_throw_misc[ability] +
                    dnd_rules_core_exhaustion_penalty(character));
}

int8_t dnd_rules_core_skill_base_modifier(const PocketCharacter* character, uint8_t skill) {
    if(skill >= POCKET_D20_SKILL_COUNT) return 0;
    uint8_t ability = dnd_rules_core_skill_abilities[skill];
    int8_t base = dnd_rules_core_ability_modifier(character->ability_scores[ability]);
    return (int8_t)(dnd_rules_core_apply_proficiency(
                        base,
                        character->skill_proficiency[skill],
                        dnd_rules_core_proficiency_bonus(character)) +
                    character->skill_misc[skill]);
}

int8_t dnd_rules_core_skill_modifier(const PocketCharacter* character, uint8_t skill) {
    return (int8_t)(dnd_rules_core_skill_base_modifier(character, skill) +
                    dnd_rules_core_exhaustion_penalty(character));
}

uint16_t dnd_rules_core_roll_dice(uint8_t count, uint8_t sides) {
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < count; ++i)
        total += dnd_rules_core_roll_die(sides);
    return total;
}
