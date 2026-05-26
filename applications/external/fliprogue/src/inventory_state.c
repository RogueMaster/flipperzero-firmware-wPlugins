#include "inventory_state.h"

#include "equipment.h"

#include <string.h>

void fr_remove_inventory(FrGame* game, uint8_t index) {
    if(index >= game->player.inv_count) return;
    for(uint8_t i = index; i + 1 < game->player.inv_count; i++) game->player.inv[i] = game->player.inv[i + 1];
    game->player.inv_count--;
    memset(&game->player.inv[game->player.inv_count], 0, sizeof(game->player.inv[0]));
}

void fr_consume_inventory(FrGame* game, uint8_t index) {
    if(index >= game->player.inv_count) return;
    if((game->player.inv[index].type == FR_ITEM_POTION ||
        game->player.inv[index].type == FR_ITEM_SCROLL ||
        game->player.inv[index].type == FR_ITEM_THROWABLE) &&
       game->player.inv[index].amount > 1) {
        game->player.inv[index].amount--;
        return;
    }
    fr_remove_inventory(game, index);
}

uint8_t fr_inventory_used_count(const FrGame* game) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        const FrInvSlot* slot = &game->player.inv[i];
        count = (uint8_t)(count + ((slot->type == FR_ITEM_POTION || slot->type == FR_ITEM_SCROLL) ? slot->amount : 1));
    }
    return count;
}

bool fr_add_inventory(FrGame* game, uint8_t type, uint8_t subtype, uint8_t amount) {
    uint8_t item_count = (type == FR_ITEM_POTION || type == FR_ITEM_SCROLL) ? amount : 1;
    if((uint16_t)fr_inventory_used_count(game) + item_count > FR_INV_CAP) return false;
    if(type == FR_ITEM_POTION || type == FR_ITEM_SCROLL || type == FR_ITEM_THROWABLE) {
        for(uint8_t i = 0; i < game->player.inv_count; i++) {
            FrInvSlot* slot = &game->player.inv[i];
            if(slot->type == type && slot->subtype == subtype && slot->amount < 99) {
                uint16_t cap = type == FR_ITEM_THROWABLE ? 20u : 99u;
                uint16_t stacked = (uint16_t)slot->amount + amount;
                slot->amount = stacked > cap ? (uint8_t)cap : (uint8_t)stacked;
                return true;
            }
        }
    }
    if(game->player.inv_count >= FR_INV_CAP) return false;
    FrInvSlot* slot = &game->player.inv[game->player.inv_count++];
    slot->type = type;
    slot->subtype = subtype;
    slot->amount = amount;
    if(type == FR_ITEM_WAND) slot->flags = fr_wand_max_charges(subtype);
    else if(type == FR_ITEM_TRINKET) slot->flags = fr_trinket_break_turns(subtype) > 0 ? FR_INV_CURSED : 0;
    else slot->flags = 0;
    return true;
}
