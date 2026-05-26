#include "chest_actions.h"

#include "actor_state.h"
#include "combat.h"
#include "game_core.h"
#include "inventory_state.h"
#include "item_defs.h"
#include "map_state.h"
#include "monster_defs.h"
#include "status_effects.h"
#include "turns.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool fr_chest_tile_ok(const FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    return fr_is_walkable(terrain) && terrain != FR_TERR_STAIRS_DOWN && terrain != FR_TERR_STAIRS_UP &&
           terrain != FR_TERR_WATER;
}

bool fr_place_chest(FrGame* game, uint8_t x, uint8_t y, bool mimic) {
    if(!fr_chest_tile_ok(game, x, y)) return false;
    if(game->player.x == x && game->player.y == y) return false;
    if(fr_chest_at(game, x, y) != NULL || fr_actor_at(game, x, y) != NULL || fr_trap_at(game, x, y) != NULL) {
        return false;
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        FrItem* item = &game->items[i];
        if(item->active) continue;
        memset(item, 0, sizeof(*item));
        item->active = true;
        item->type = FR_ITEM_CHEST;
        item->subtype = 0;
        item->x = x;
        item->y = y;
        item->amount = 1;
        item->flags = mimic ? FR_ITEM_FLAG_MIMIC : 0;
        return true;
    }
    return false;
}

FrItem* fr_chest_at(FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        FrItem* item = &game->items[i];
        if(item->active && item->type == FR_ITEM_CHEST && item->x == x && item->y == y) return item;
    }
    return NULL;
}

FrItem* fr_adjacent_chest_for_bump(FrGame* game, uint8_t x, uint8_t y) {
    FrItem* chest = fr_chest_at(game, x, y);
    if(!chest) return NULL;
    uint8_t dx = (uint8_t)abs((int)game->player.x - (int)x);
    uint8_t dy = (uint8_t)abs((int)game->player.y - (int)y);
    return dx + dy == 1 ? chest : NULL;
}

uint8_t fr_chest_choice_count(const FrGame* game, const FrItem* chest) {
    (void)game;
    if(!chest || !chest->active || chest->type != FR_ITEM_CHEST) return 0;
    if((chest->flags & FR_ITEM_FLAG_OPENED) != 0) return 0;
    return (chest->flags & FR_ITEM_FLAG_MIMIC) != 0 ? 0 : 3;
}

static uint8_t fr_chest_roll(const FrGame* game, const FrItem* chest, uint8_t choice, uint8_t limit) {
    uint32_t value = game->run_seed ^ ((uint32_t)game->floor * 1103515245u);
    value ^= ((uint32_t)chest->x + 17u) * 2654435761u;
    value ^= ((uint32_t)chest->y + 31u) * 2246822519u;
    value ^= ((uint32_t)choice + 1u) * 3266489917u;
    return limit == 0 ? 0 : (uint8_t)(value % limit);
}

FrInvSlot fr_chest_choice_slot(const FrGame* game, const FrItem* chest, uint8_t choice) {
    if(fr_chest_choice_count(game, chest) == 0 || choice >= 3) return (FrInvSlot){0};
    switch(choice) {
    case 0:
        return (FrInvSlot){
            FR_ITEM_POTION,
            (uint8_t)(1 + fr_chest_roll(game, chest, choice, FR_POTION_MAX - 1)),
            1,
            0};
    case 1:
        return (FrInvSlot){
            FR_ITEM_SCROLL,
            (uint8_t)(1 + fr_chest_roll(game, chest, choice, FR_SCROLL_MAX - 1)),
            1,
            0};
    default:
        if(fr_chest_roll(game, chest, choice, 2) == 0) {
            return (FrInvSlot){
                FR_ITEM_WAND,
                (uint8_t)(1 + fr_chest_roll(game, chest, choice, FR_WAND_MAX - 1)),
                2,
                0};
        }
        return (FrInvSlot){
            FR_ITEM_TRINKET,
            (uint8_t)(1 + fr_chest_roll(game, chest, choice, FR_TRINKET_MAX - 1)),
            1,
            0};
    }
}

static FrActionResult fr_activate_mimic_chest(FrGame* game, FrItem* chest) {
    uint8_t x = chest->x;
    uint8_t y = chest->y;
    chest->active = false;
    FrActor* mimic = fr_spawn_actor(game, FR_MON_MIMIC, x, y);
    if(mimic) fr_reveal_actor(mimic);
    uint8_t damage = fr_monster_def(FR_MON_MIMIC)->damage;
    uint8_t block = fr_player_block(game);
    damage = damage > block ? (uint8_t)(damage - block) : 1;
    if(game->player.hp > damage) {
        game->player.hp = (uint8_t)(game->player.hp - damage);
        fr_log(game, "Oh no. Mimic!");
    } else {
        game->player.hp = 0;
        fr_set_game_over(game, FR_DEATH_KILLED, "Killed by Mimic.");
    }
    fr_finish_item_turn(game);
    return (FrActionResult){FR_ACTION_ATTACK};
}

FrActionResult fr_bump_chest(FrGame* game, FrItem* chest) {
    game->log[0] = '\0';
    if(!chest || !chest->active || chest->type != FR_ITEM_CHEST) return (FrActionResult){FR_ACTION_BLOCKED};
    if((chest->flags & FR_ITEM_FLAG_OPENED) != 0) {
        fr_log(game, "Empty chest.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    if((chest->flags & FR_ITEM_FLAG_MIMIC) != 0) return fr_activate_mimic_chest(game, chest);
    fr_log(game, "Chest. Press OK.");
    return (FrActionResult){FR_ACTION_BLOCKED};
}

FrActionResult fr_open_chest_choice(FrGame* game, FrItem* chest, uint8_t choice) {
    game->log[0] = '\0';
    if((chest->flags & FR_ITEM_FLAG_MIMIC) != 0) return fr_activate_mimic_chest(game, chest);
    if(fr_chest_choice_count(game, chest) == 0 || choice >= 3) return (FrActionResult){FR_ACTION_BLOCKED};

    FrInvSlot slot = fr_chest_choice_slot(game, chest, choice);
    if(!fr_add_inventory(game, slot.type, slot.subtype, slot.amount)) {
        fr_log(game, "Pack full.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    chest->flags |= FR_ITEM_FLAG_OPENED;
    fr_log(game, "Chest opens.");
    fr_finish_item_turn(game);
    return (FrActionResult){FR_ACTION_USE};
}

FrActionResult fr_cancel_chest(FrGame* game, FrItem* chest) {
    (void)chest;
    fr_log(game, "Chest waits.");
    return (FrActionResult){FR_ACTION_BLOCKED};
}
