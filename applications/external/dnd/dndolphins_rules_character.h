#pragma once

#include "dnd_rules.h"

uint32_t dndolphins_rules_character_minimum_experience_for_level(uint8_t level);
void dndolphins_rules_character_apply_experience_floor(PocketCharacter* character);
void dndolphins_rules_character_apply_level_increase(
    PocketCharacter* character, uint8_t class_index, uint8_t previous_class_level);
int8_t dndolphins_rules_character_initiative_modifier(const PocketCharacter* character);
int16_t dndolphins_rules_character_effective_speed(const PocketCharacter* character);
int16_t dndolphins_rules_character_feature_max_uses(
    const PocketCharacter* character, const PocketFeature* feature);
void dndolphins_rules_character_short_rest(PocketCharacter* character);
int16_t dndolphins_rules_character_spend_class_hit_die(
    PocketCharacter* character,
    uint8_t class_index,
    uint8_t* die_roll);
void dndolphins_rules_character_long_rest(PocketCharacter* character);
