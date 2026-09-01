#pragma once

#include "dnd_weapon_rules.h"
#include "dnd_storage.h"
#include "dndolphins_dice.h"

#include <stdbool.h>
#include <stdint.h>

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

PocketAttackRoll dndolphins_weapon_combat_roll_attack(
    const PocketCharacter* character,
    const PocketItem* item,
    PocketRollMode mode);
PocketDamageRoll dndolphins_weapon_combat_roll_damage(
    const PocketCharacter* character,
    const PocketItem* item,
    bool critical);
bool dndolphins_weapon_combat_items_collect_weapon_indices(
    Storage* storage,
    uint32_t profile,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count);
