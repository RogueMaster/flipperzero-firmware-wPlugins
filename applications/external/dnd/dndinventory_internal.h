#pragma once

#include "dnd_rules.h"
#include "dnd_storage.h"

#include <stdbool.h>
#include <stdint.h>

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
    PocketSaveData* data,
    bool* initialized);
bool dndinventory_items_regrant_inventory_once(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* regranted);

int16_t dndinventory_rules_carrying_capacity(const PocketCharacter* character);
void dndinventory_rules_normalize_currency(PocketCharacter* character);
int16_t dndinventory_rules_calculated_armor_class(
    const PocketCharacter* character,
    const DndInventoryItemAggregate* aggregate);
