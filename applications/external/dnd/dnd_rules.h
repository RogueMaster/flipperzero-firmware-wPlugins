#pragma once

#include "dnd_data.h"

extern const char* const dnd_rules_core_ability_names[DND_ABILITY_COUNT];
extern const char* const dnd_rules_core_skill_names[DND_SKILL_COUNT];
extern const uint8_t dnd_rules_core_skill_abilities[DND_SKILL_COUNT];
extern const char* const dnd_rules_core_damage_names[DndDamageTypeCount];

int8_t dnd_rules_core_ability_modifier(int8_t score);
uint8_t dnd_rules_core_total_level(const DndCharacter* character);
uint8_t dnd_rules_core_proficiency_bonus(const DndCharacter* character);
int8_t dnd_rules_core_saving_throw_modifier(const DndCharacter* character, uint8_t ability);
int8_t dnd_rules_core_skill_modifier(const DndCharacter* character, uint8_t skill);
int8_t dnd_rules_core_skill_base_modifier(const DndCharacter* character, uint8_t skill);
int8_t dnd_rules_core_exhaustion_penalty(const DndCharacter* character);

uint8_t dnd_rules_core_roll_die(uint8_t sides);
uint16_t dnd_rules_core_roll_dice(uint8_t count, uint8_t sides);
