#pragma once

#include "dndolphins_rules.h"
#include "dndolphins_storage.h"

#include <stdbool.h>
#include <stdint.h>

/* Inventory initialization is feature-owned: character creation leaves the item
   sidecar absent, and DNDolphins calls this only when the Inventory UI is opened. */
bool pocket_d20_items_initialize_inventory(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* initialized);

/* Adventure and other feature modules may grant a real item without knowing how
   starting equipment is composed. This does not seed starting equipment. */
bool pocket_d20_items_grant_reward(
    Storage* storage,
    uint32_t profile,
    PocketCharacter* character,
    const char* name,
    const char* detail);

int16_t pocket_d20_carrying_capacity(const PocketCharacter* character);
void pocket_d20_normalize_currency(PocketCharacter* character);
int8_t pocket_d20_weapon_ability(const PocketCharacter* character, const PocketItem* item);
int8_t pocket_d20_weapon_attack_modifier(const PocketCharacter* character, const PocketItem* item);
PocketAttackRoll pocket_d20_roll_attack(
    const PocketCharacter* character,
    const PocketItem* item,
    PocketRollMode mode);
PocketDamageRoll
    pocket_d20_roll_damage(const PocketCharacter* character, const PocketItem* item, bool critical);

int16_t pocket_d20_items_calculated_armor_class(
    const PocketCharacter* character,
    const PocketD20ItemAggregate* aggregate);

bool pocket_d20_items_collect_weapon_indices(
    Storage* storage,
    uint32_t profile,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count);
