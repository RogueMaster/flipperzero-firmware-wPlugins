#include "equipment.h"

#include <stddef.h>

uint8_t fr_wand_max_charges(uint8_t wand) {
    switch(wand) {
    case FR_WAND_BLINK:
    case FR_WAND_REVEAL:
        return 2;
    default:
        return 3;
    }
}

uint8_t fr_trinket_break_turns(uint8_t trinket) {
    return trinket == FR_TRINKET_CINDER || trinket == FR_TRINKET_GLASS || trinket == FR_TRINKET_HUNGRY ? 250 : 0;
}

FrInvSlot* fr_equipped_trinket_slot(FrGame* game) {
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        FrInvSlot* slot = &game->player.inv[i];
        if(slot->type == FR_ITEM_TRINKET && (slot->flags & FR_INV_EQUIPPED) != 0) return slot;
    }
    return NULL;
}

bool fr_has_equipped_trinket(const FrGame* game, uint8_t trinket) {
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        const FrInvSlot* slot = &game->player.inv[i];
        if(slot->type == FR_ITEM_TRINKET && slot->subtype == trinket &&
           (slot->flags & FR_INV_EQUIPPED) != 0) {
            return true;
        }
    }
    return false;
}
