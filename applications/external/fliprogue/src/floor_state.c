#include "floor_state.h"

#include "monster_defs.h"

#include <string.h>

void fr_saved_actor_from_actor(FrSavedActor* saved, const FrActor* actor) {
    memset(saved, 0, sizeof(*saved));
    if(!actor->active) return;
    saved->active = true;
    saved->type = actor->type;
    saved->x = actor->x;
    saved->y = actor->y;
    saved->hp = actor->hp;
    saved->effects = actor->effects;
    memcpy(saved->fx_timer, actor->fx_timer, sizeof(saved->fx_timer));
    saved->flags = actor->flags;
    saved->pack_id = actor->pack_id;
    saved->cooldown = actor->cooldown;
}

void fr_actor_from_saved_actor(FrActor* actor, const FrSavedActor* saved) {
    memset(actor, 0, sizeof(*actor));
    if(!saved->active) return;
    const FrMonsterDef* def = fr_monster_def(saved->type);
    actor->active = true;
    actor->type = saved->type;
    actor->x = saved->x;
    actor->y = saved->y;
    actor->target_x = saved->x;
    actor->target_y = saved->y;
    actor->hp = saved->hp;
    actor->max_hp = def->hp;
    actor->dmg = def->damage;
    actor->effects = saved->effects;
    memcpy(actor->fx_timer, saved->fx_timer, sizeof(actor->fx_timer));
    actor->flags = saved->flags;
    actor->pack_id = saved->pack_id;
    actor->cooldown = saved->cooldown;
    if(saved->type == FR_MON_YONDER_WARDEN) actor->memory = 255;
}

void fr_saved_item_from_item(FrSavedItem* saved, const FrItem* item) {
    memset(saved, 0, sizeof(*saved));
    if(!item->active) return;
    saved->type = item->type;
    saved->subtype = item->subtype;
    saved->x = item->x;
    saved->y = item->y;
    saved->amount = item->amount;
    saved->flags = item->flags;
}

void fr_item_from_saved_item(FrItem* item, const FrSavedItem* saved) {
    memset(item, 0, sizeof(*item));
    if(saved->type == FR_ITEM_NONE) return;
    item->active = true;
    item->type = saved->type;
    item->subtype = saved->subtype;
    item->x = saved->x;
    item->y = saved->y;
    item->amount = saved->amount;
    item->flags = saved->flags;
}

void fr_save_current_floor(FrGame* game) {
    if(game->floor == 0 || game->floor > FR_MAX_FLOORS) return;
    FrFloorState* floor = &game->floors[game->floor - 1];
    floor->generated = true;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        fr_saved_actor_from_actor(&floor->actors[i], &game->actors[i]);
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        fr_saved_item_from_item(&floor->items[i], &game->items[i]);
    }
    memcpy(floor->traps, game->traps, sizeof(game->traps));
    memset(floor->explored, 0, sizeof(floor->explored));
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if((game->tiles[y][x] & FR_TILE_EXPLORED) != 0) {
                uint16_t pos = (uint16_t)(y / 2) * FR_FLOOR_EXPLORED_W + (uint8_t)(x / 2);
                floor->explored[pos / 8] |= (uint8_t)(1u << (pos % 8));
            }
        }
    }
}

bool fr_floor_actor_at_except(const FrFloorState* floor, uint8_t x, uint8_t y, uint8_t except_index) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(i == except_index) continue;
        if(floor->actors[i].active && floor->actors[i].x == x && floor->actors[i].y == y) return true;
    }
    return false;
}

void fr_init_floor_state(FrGame* game, uint8_t floor_id, uint8_t up_x, uint8_t up_y, uint8_t down_x, uint8_t down_y) {
    if(floor_id == 0 || floor_id > FR_MAX_FLOORS) return;
    FrFloorState* floor = &game->floors[floor_id - 1];
    memset(floor, 0, sizeof(*floor));
    floor->generated = true;
    floor->up_x = up_x;
    floor->up_y = up_y;
    floor->down_x = down_x;
    floor->down_y = down_y;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        fr_saved_actor_from_actor(&floor->actors[i], &game->actors[i]);
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        fr_saved_item_from_item(&floor->items[i], &game->items[i]);
    }
    memcpy(floor->traps, game->traps, sizeof(game->traps));
    fr_save_current_floor(game);
}

void fr_apply_saved_floor_state(FrGame* game, uint8_t floor_id) {
    FrFloorState* floor = &game->floors[floor_id - 1];
    uint8_t old_suppress = game->suppress_deltas;
    game->suppress_deltas = 1;
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            game->tiles[y][x] &= (uint8_t) ~(FR_TILE_VISIBLE | FR_TILE_EXPLORED);
        }
    }
    for(uint8_t i = 0; i < floor->tile_delta_count; i++) {
        uint16_t pos = floor->tile_delta_pos[i];
        uint8_t x = (uint8_t)(pos % FR_MAP_W);
        uint8_t y = (uint8_t)(pos / FR_MAP_W);
        game->tiles[y][x] =
            (uint8_t)((game->tiles[y][x] & ~(FR_TILE_TERRAIN_MASK | FR_TILE_HIDDEN_DOOR)) |
                      floor->tile_delta_tile[i]);
    }
    for(uint16_t pos = 0; pos < FR_FLOOR_EXPLORED_W * FR_FLOOR_EXPLORED_H; pos++) {
        if((floor->explored[pos / 8] & (uint8_t)(1u << (pos % 8))) != 0) {
            uint8_t cell_x = (uint8_t)((pos % FR_FLOOR_EXPLORED_W) * 2);
            uint8_t cell_y = (uint8_t)((pos / FR_FLOOR_EXPLORED_W) * 2);
            for(uint8_t oy = 0; oy < 2; oy++) {
                for(uint8_t ox = 0; ox < 2; ox++) {
                    uint8_t x = (uint8_t)(cell_x + ox);
                    uint8_t y = (uint8_t)(cell_y + oy);
                    if(x < FR_MAP_W && y < FR_MAP_H && fr_get_terrain(game, x, y) != FR_TERR_WALL) {
                        game->tiles[y][x] |= FR_TILE_EXPLORED;
                    }
                }
            }
        }
    }
    game->suppress_deltas = old_suppress;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        fr_actor_from_saved_actor(&game->actors[i], &floor->actors[i]);
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        fr_item_from_saved_item(&game->items[i], &floor->items[i]);
    }
    memcpy(game->traps, floor->traps, sizeof(game->traps));
}

void fr_enter_floor(FrGame* game, uint8_t floor, uint8_t entry_stairs) {
    bool restore_saved = floor >= 1 && floor <= FR_MAX_FLOORS && game->floors[floor - 1].generated;
    uint8_t old_suppress_floor_state_save = game->suppress_floor_state_save;
    if(restore_saved) game->suppress_floor_state_save = 1;
    fr_generate_floor(game, floor);
    game->suppress_floor_state_save = old_suppress_floor_state_save;
    if(restore_saved) {
        fr_apply_saved_floor_state(game, floor);
    }

    uint8_t entry_x = 0;
    uint8_t entry_y = 0;
    if(fr_find_first_tile(game, entry_stairs, &entry_x, &entry_y)) {
        game->player.x = entry_x;
        game->player.y = entry_y;
    }
    fr_update_fov(game);
}
