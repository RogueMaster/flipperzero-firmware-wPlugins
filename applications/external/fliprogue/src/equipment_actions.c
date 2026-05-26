#include "equipment_actions.h"

#include "equipment.h"
#include "game_core.h"
#include "item_defs.h"

bool fr_equip_trinket(FrGame* game, uint8_t index) {
    if(index >= game->player.inv_count || game->player.inv[index].type != FR_ITEM_TRINKET) return false;
    FrInvSlot* equipped = fr_equipped_trinket_slot(game);
    if(equipped == &game->player.inv[index]) {
        if((equipped->flags & FR_INV_CURSED) != 0) {
            fr_log(game, "It clings.");
            return false;
        }
        equipped->flags &= (uint8_t)~FR_INV_EQUIPPED;
        fr_log(game, "Charm off.");
        return true;
    }
    if(equipped && (equipped->flags & FR_INV_CURSED) != 0) {
        fr_log(game, "Old charm clings.");
        return false;
    }
    if(equipped) equipped->flags &= (uint8_t)~FR_INV_EQUIPPED;
    game->player.inv[index].flags |= FR_INV_EQUIPPED;
    fr_mark_known_trinket(game, game->player.inv[index].subtype);
    fr_log(game, "You wear %s.", fr_trinket_label(game, game->player.inv[index].subtype));
    return true;
}
