#pragma once

#include "dnd_profile_projection.h"
#include "dnd_rules.h"
#include "dnd_storage.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    char species[DND_NAME_LEN];
    char background[DND_NAME_LEN];
    uint8_t class_count;
    DndClassLevel classes[DND_MAX_CLASSES];
    int8_t ability_scores[DND_ABILITY_COUNT];
    int16_t armor_class;
    uint8_t exhaustion;
    uint8_t encumbrance_mode;
    int16_t carrying_capacity_override;
    int32_t currency_cp;
    int32_t currency_sp;
    int32_t currency_ep;
    int32_t currency_gp;
    int32_t currency_pp;
    uint8_t item_count;
    DndItem* items;
} DndInventoryCharacterState;

typedef struct {
    DndInventoryCharacterState character;
} DndInventoryAppData;

typedef struct {
    int16_t carried_weight_tenths;
    int16_t equipped_weight_tenths;
    uint8_t attuned_count;
    uint8_t armor_base;
    int8_t armor_dex_cap;
    uint8_t shield_bonus;
} DndInventoryItemAggregate;

/* DNDInventory-only initialization. Merely opening DNDolphins never creates or
   seeds an Inventory sidecar. */
bool dndinventory_items_initialize_inventory(
    Storage* storage,
    uint32_t profile,
    DndInventoryCharacterState* character,
    bool* initialized);
bool dndinventory_items_regrant_inventory_once(
    Storage* storage,
    uint32_t profile,
    DndInventoryCharacterState* character,
    bool* regranted);

int16_t dndinventory_rules_carrying_capacity(const DndInventoryCharacterState* character);
void dndinventory_rules_normalize_currency(DndInventoryCharacterState* character);
int16_t dndinventory_rules_calculated_armor_class(
    const DndInventoryCharacterState* character,
    const DndInventoryItemAggregate* aggregate);
int8_t dndinventory_rules_weapon_attack_modifier(
    const DndInventoryCharacterState* character,
    const DndItem* item);
