#include "item_identity.h"

#include "game_core.h"

bool fr_identify_one_unknown(FrGame* game, uint8_t skip_index) {
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        if(i == skip_index) continue;
        FrInvSlot* other = &game->player.inv[i];
        if(other->type == FR_ITEM_POTION && !fr_knows_potion(game, other->subtype)) {
            fr_mark_known_potion(game, other->subtype);
            return true;
        }
        if(other->type == FR_ITEM_SCROLL && !fr_knows_scroll(game, other->subtype)) {
            fr_mark_known_scroll(game, other->subtype);
            return true;
        }
        if(other->type == FR_ITEM_TRINKET && !fr_knows_trinket(game, other->subtype)) {
            fr_mark_known_trinket(game, other->subtype);
            return true;
        }
    }
    return false;
}

bool fr_identify_inventory_slot(FrGame* game, uint8_t target_index) {
    if(target_index >= game->player.inv_count) return false;
    FrInvSlot* other = &game->player.inv[target_index];
    if(other->type == FR_ITEM_POTION && !fr_knows_potion(game, other->subtype)) {
        fr_mark_known_potion(game, other->subtype);
        return true;
    }
    if(other->type == FR_ITEM_SCROLL && !fr_knows_scroll(game, other->subtype)) {
        fr_mark_known_scroll(game, other->subtype);
        return true;
    }
    if(other->type == FR_ITEM_TRINKET && !fr_knows_trinket(game, other->subtype)) {
        fr_mark_known_trinket(game, other->subtype);
        return true;
    }
    return false;
}
