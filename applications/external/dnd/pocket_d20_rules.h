#pragma once

#include "pocket_d20.h"

typedef enum {
    PocketRollNormal,
    PocketRollAdvantage,
    PocketRollDisadvantage,
    PocketRollGuidance,
} PocketRollMode;

#define POCKET_D20_MAX_DAMAGE_ROLLS 80U

typedef struct {
    uint8_t first_die;
    uint8_t second_die;
    int16_t modifier;
    int16_t total;
    uint8_t natural_roll;
    uint8_t critical;
    uint8_t automatic_miss;
} PocketAttackRoll;

typedef struct {
    int16_t weapon_total;
    int16_t extra_total;
    int16_t modifier;
    int16_t total;
    uint8_t critical;
    uint8_t weapon_roll_count;
    uint8_t extra_roll_count;
    uint8_t rolls[POCKET_D20_MAX_DAMAGE_ROLLS];
} PocketDamageRoll;

extern const char* const pocket_d20_ability_names[POCKET_D20_ABILITY_COUNT];
extern const char* const pocket_d20_skill_names[POCKET_D20_SKILL_COUNT];
extern const uint8_t pocket_d20_skill_abilities[POCKET_D20_SKILL_COUNT];
extern const char* const pocket_d20_damage_names[PocketDamageTypeCount];
extern const char* const pocket_d20_journal_category_names[PocketJournalCategoryCount];

int8_t pocket_d20_ability_modifier(int8_t score);
uint8_t pocket_d20_total_level(const PocketCharacter* character);
uint8_t pocket_d20_proficiency_bonus(const PocketCharacter* character);
int8_t pocket_d20_saving_throw_modifier(const PocketCharacter* character, uint8_t ability);
int8_t pocket_d20_skill_modifier(const PocketCharacter* character, uint8_t skill);
int8_t pocket_d20_skill_base_modifier(const PocketCharacter* character, uint8_t skill);
int8_t pocket_d20_initiative_modifier(const PocketCharacter* character);
int16_t pocket_d20_effective_speed(const PocketCharacter* character);
int8_t pocket_d20_spell_attack_modifier(const PocketCharacter* character);
int8_t pocket_d20_spell_save_dc(const PocketCharacter* character);
uint8_t pocket_d20_class_prepared_count(const PocketCharacter* character, uint8_t class_index);
uint8_t pocket_d20_class_known_count(const PocketCharacter* character, uint8_t class_index);
void pocket_d20_recalculate_multiclass_slots(PocketCharacter* character);
int16_t pocket_d20_carried_weight_tenths(const PocketCharacter* character);
int16_t pocket_d20_equipped_weight_tenths(const PocketCharacter* character);
int16_t pocket_d20_carrying_capacity(const PocketCharacter* character);
uint8_t pocket_d20_attuned_count(const PocketCharacter* character);
int16_t pocket_d20_calculated_armor_class(const PocketCharacter* character);
void pocket_d20_normalize_currency(PocketCharacter* character);
int16_t pocket_d20_feature_max_uses(const PocketCharacter* character, const PocketFeature* feature);
int8_t pocket_d20_weapon_ability(const PocketCharacter* character, const PocketItem* item);
int8_t pocket_d20_weapon_attack_modifier(
    const PocketCharacter* character,
    const PocketItem* item);

uint16_t pocket_d20_roll_dice(uint8_t count, uint8_t sides);
uint16_t pocket_d20_roll_dice_values(
    uint8_t count,
    uint8_t sides,
    uint8_t* values,
    uint8_t capacity);
PocketAttackRoll pocket_d20_roll_attack(
    const PocketCharacter* character,
    const PocketItem* item,
    PocketRollMode mode);
PocketDamageRoll pocket_d20_roll_damage(
    const PocketCharacter* character,
    const PocketItem* item,
    bool critical);

void pocket_d20_short_rest(PocketCharacter* character);
int16_t pocket_d20_spend_hit_die(PocketCharacter* character, uint8_t* die_roll);
int16_t pocket_d20_spend_class_hit_die(
    PocketCharacter* character,
    uint8_t class_index,
    uint8_t* die_roll);
void pocket_d20_long_rest(PocketCharacter* character);
