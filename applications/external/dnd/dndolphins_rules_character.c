#include "dnd_rules.h"
#include "dndolphins_rules_character.h"

uint32_t dndolphins_rules_character_minimum_experience_for_level(uint8_t level) {
    static const uint32_t minimum_xp[20] = {
        0U,      300U,    900U,    2700U,   6500U,   14000U, 23000U,
        34000U,  48000U,  64000U,  85000U,  100000U, 120000U, 140000U,
        165000U, 195000U, 225000U, 265000U, 305000U, 355000U,
    };
    if(level < 1U) level = 1U;
    if(level > 20U) level = 20U;
    return minimum_xp[level - 1U];
}

void dndolphins_rules_character_apply_experience_floor(PocketCharacter* character) {
    if(!character) return;
    uint32_t minimum = dndolphins_rules_character_minimum_experience_for_level(dnd_rules_core_total_level(character));
    if(character->experience < minimum) character->experience = minimum;
}

void dndolphins_rules_character_apply_level_increase(
    PocketCharacter* character, uint8_t class_index, uint8_t previous_class_level) {
    if(!character || class_index >= character->class_count) return;
    PocketClassLevel* class_level = &character->classes[class_index];
    if(class_level->level <= previous_class_level) return;

    const uint8_t levels_gained = (uint8_t)(class_level->level - previous_class_level);
    uint8_t hit_die = class_level->hit_die;
    if(hit_die < 2U) hit_die = 2U;
    const int16_t constitution_modifier = dnd_rules_core_ability_modifier(
        character->ability_scores[PocketAbilityConstitution]);
    int16_t hp_per_level = (int16_t)(hit_die / 2U + 1U) + constitution_modifier;
    if(hp_per_level < 1) hp_per_level = 1;

    int32_t hp_gain = (int32_t)hp_per_level * levels_gained;
    int32_t new_max = (int32_t)character->hp_max + hp_gain;
    if(new_max > 999) new_max = 999;
    if(new_max < 1) new_max = 1;
    hp_gain = new_max - character->hp_max;
    character->hp_max = (int16_t)new_max;
    if(hp_gain > 0) {
        int32_t new_current = (int32_t)character->hp_current + hp_gain;
        if(new_current > character->hp_max) new_current = character->hp_max;
        if(new_current < -999) new_current = -999;
        character->hp_current = (int16_t)new_current;
    }

    /* One Hit Die is gained for each class level. Leveling also refreshes the
       available pool to its new maximum, matching the requested character-sheet
       behavior without adding any persisted progression state. */
    class_level->hit_dice_max = class_level->level;
    class_level->hit_dice_current = class_level->hit_dice_max;
    character->hit_dice_max = dnd_rules_core_total_level(character);
    character->hit_dice_current = character->hit_dice_max;
}

int8_t dndolphins_rules_character_initiative_modifier(const PocketCharacter* character) {
    return (int8_t)(dnd_rules_core_ability_modifier(
                        character->ability_scores[PocketAbilityDexterity]) +
                    character->initiative_misc + dnd_rules_core_exhaustion_penalty(character));
}

int16_t dndolphins_rules_character_effective_speed(const PocketCharacter* character) {
    int16_t speed = character->speed - (5 * character->exhaustion);
    return speed > 0 ? speed : 0;
}

int16_t dndolphins_rules_character_feature_max_uses(
    const PocketCharacter* character, const PocketFeature* feature) {
    if(feature->resource_formula == PocketResourceProficiency)
        return dnd_rules_core_proficiency_bonus(character);
    if(feature->resource_formula == PocketResourceAbility &&
       feature->resource_ability < POCKET_D20_ABILITY_COUNT) {
        int16_t modifier =
            dnd_rules_core_ability_modifier(character->ability_scores[feature->resource_ability]);
        return modifier > 0 ? modifier : 1;
    }
    return feature->uses_max;
}

void dndolphins_rules_character_short_rest(PocketCharacter* character) {
    for(uint8_t i = 0U; i < character->feature_count; ++i) {
        if(character->features[i].recharge == PocketRechargeShortOrLong)
            character->features[i].uses_current =
                dndolphins_rules_character_feature_max_uses(character, &character->features[i]);
    }
}

int16_t dndolphins_rules_character_spend_class_hit_die(
    PocketCharacter* character,
    uint8_t class_index,
    uint8_t* die_roll) {
    if(class_index >= character->class_count) return -1;
    PocketClassLevel* class_level = &character->classes[class_index];
    if(character->hp_current < 1 || character->hp_current >= character->hp_max ||
       class_level->hit_dice_current == 0U)
        return -1;
    uint8_t roll = dnd_rules_core_roll_die(class_level->hit_die);
    int16_t healing =
        roll + dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityConstitution]);
    if(healing < 1) healing = 1;
    int16_t missing = character->hp_max - character->hp_current;
    int16_t regained = healing < missing ? healing : missing;
    character->hp_current += regained;
    --class_level->hit_dice_current;
    if(die_roll) *die_roll = roll;
    return regained;
}

void dndolphins_rules_character_long_rest(PocketCharacter* character) {
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
    for(uint8_t i = 1U; i < POCKET_D20_SLOT_COUNT; ++i)
        character->spell_slots_current[i] = character->spell_slots_max[i];
    for(uint8_t i = 0U; i < character->feature_count; ++i)
        if(character->features[i].recharge != PocketRechargeManual &&
           character->features[i].recharge != PocketRechargeTurn &&
           character->features[i].recharge != PocketRechargeEncounter)
            character->features[i].uses_current =
                dndolphins_rules_character_feature_max_uses(character, &character->features[i]);
}
