#include "pickup_actions.h"

#include "actor_state.h"
#include "equipment.h"
#include "game_core.h"
#include "grate_actions.h"
#include "inventory_state.h"
#include "item_defs.h"
#include "placement.h"

bool fr_item_at_xy(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active && game->items[i].x == x && game->items[i].y == y) return true;
    }
    return false;
}

bool fr_drop_inventory_item(FrGame* game, uint8_t index) {
    if(index >= game->player.inv_count) return false;
    if(fr_item_at_xy(game, game->player.x, game->player.y)) return false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active) continue;
        FrInvSlot* slot = &game->player.inv[index];
        game->items[i].active = true;
        game->items[i].type = slot->type;
        game->items[i].subtype = slot->subtype;
        game->items[i].x = game->player.x;
        game->items[i].y = game->player.y;
        game->items[i].amount =
            (slot->type == FR_ITEM_POTION || slot->type == FR_ITEM_SCROLL ||
             slot->type == FR_ITEM_THROWABLE) ?
                1 :
                slot->amount;
        game->items[i].flags = slot->flags;
        if((slot->type == FR_ITEM_POTION || slot->type == FR_ITEM_SCROLL ||
            slot->type == FR_ITEM_THROWABLE) &&
           slot->amount > 1) {
            slot->amount--;
        } else {
            fr_remove_inventory(game, index);
        }
        return true;
    }
    return false;
}

static const char* fr_pickup_log_label(const FrGame* game, const FrItem* item, uint8_t label_flags) {
    if(item->type == FR_ITEM_SCROLL && !fr_player_knows_scrolls(game) && !fr_knows_scroll(game, item->subtype)) {
        return game->scroll_labels[item->subtype];
    }
    return fr_inventory_label(game, &(FrInvSlot){item->type, item->subtype, item->amount, label_flags});
}

void fr_pickup_at_player(FrGame* game) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        FrItem* item = &game->items[i];
        if(!item->active || item->x != game->player.x || item->y != game->player.y) continue;

        if(item->type == FR_ITEM_GOLD) {
            game->player.gold += item->amount;
            item->active = false;
            fr_log(game, "Picked +%ug.", item->amount);
        } else if(item->type == FR_ITEM_FOOD) {
            game->player.food += item->amount;
            item->active = false;
            fr_log(game, "Food found.");
        } else if(item->type == FR_ITEM_ARROWS) {
            game->player.arrows += item->amount;
            item->active = false;
            fr_log(game, "+%u arrows.", item->amount);
        } else if(item->type == FR_ITEM_GEAR) {
            if(item->subtype == 1 && game->player.body_lvl < 3) game->player.body_lvl++;
            else if(game->player.sword_lvl < 3) game->player.sword_lvl++;
            item->active = false;
            fr_log(game, "Gear improves.");
        } else if(item->type == FR_ITEM_ORB) {
            game->player.has_orb = 1;
            item->active = false;
            fr_log(game, "Orb wakes.");
            uint8_t wx = item->x > 3 ? (uint8_t)(item->x - 3) : item->x;
            uint8_t wy = item->y;
            if(!fr_find_free_near(game, &wx, &wy)) {
                wx = item->x;
                wy = item->y > 3 ? (uint8_t)(item->y - 3) : item->y;
                fr_find_free_near(game, &wx, &wy);
            }
            FrActor* warden = fr_spawn_actor(game, FR_MON_YONDER_WARDEN, wx, wy);
            if(warden) {
                warden->target_x = game->player.x;
                warden->target_y = game->player.y;
                warden->memory = 255;
            }
        } else if(item->type == FR_ITEM_CHEST) {
            fr_log(game, "Chest here.");
        } else if(item->type == FR_ITEM_KEY) {
            item->active = false;
            if(!fr_open_grates_on_floor(game)) fr_log(game, "Key clicks.");
        } else if(fr_add_inventory(game, item->type, item->subtype, item->amount)) {
            uint8_t label_flags = item->flags;
            if(item->type == FR_ITEM_WAND && label_flags == 0) label_flags = fr_wand_max_charges(item->subtype);
            item->active = false;
            fr_log(
                game,
                "Picked %s.",
                fr_pickup_log_label(game, item, label_flags));
        } else {
            fr_log(game, "Pack full.");
        }
    }
}
