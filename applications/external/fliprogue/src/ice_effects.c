#include "ice_effects.h"

#include "actor_state.h"
#include "combat.h"
#include "game_core.h"
#include "map_state.h"
#include "placement.h"
#include "terrain_effects.h"

#include <string.h>

static bool fr_can_freeze_terrain(uint8_t terrain) {
    return terrain == FR_TERR_WATER || terrain == FR_TERR_PUDDLE;
}

static void fr_freeze_cell(FrGame* game, uint8_t floor, uint8_t x, uint8_t y) {
    if(floor != game->floor) return;
    if(!fr_can_freeze_terrain(fr_get_terrain(game, x, y))) return;
    fr_set_terrain(game, x, y, FR_TERR_ICE);
    FrActor* actor = fr_actor_at(game, x, y);
    if(actor && actor->type == FR_MON_EEL) {
        fr_kill_actor(game, actor);
        fr_log(game, "Eel freezes.");
    }
}

void fr_freeze_water_area(FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius) {
    FrTerrainField* field = fr_alloc_terrain_field(game);
    memset(field, 0, sizeof(*field));
    field->active = true;
    field->type = FR_TERRAIN_FIELD_ICE;
    field->floor = floor;
    field->ttl = 6;

    int8_t r = (int8_t)radius;
    for(int8_t oy = -r; oy <= r; oy++) {
        for(int8_t ox = -r; ox <= r; ox++) {
            int16_t x_i = (int16_t)cx + ox;
            int16_t y_i = (int16_t)cy + oy;
            if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
            uint8_t x = (uint8_t)x_i;
            uint8_t y = (uint8_t)y_i;
            if(!fr_can_freeze_terrain(fr_get_terrain(game, x, y))) continue;
            fr_freeze_cell(game, floor, x, y);
            fr_terrain_field_cell_set(field, x, y);
        }
    }
    if(!fr_terrain_field_has_any_cell(field)) field->active = false;
}

void fr_tick_ice_field(FrGame* game, FrTerrainField* field) {
    uint8_t next_cells[FR_EXPLORED_BYTES];
    memcpy(next_cells, field->cells, sizeof(next_cells));
    bool spread = false;
    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
            if(!fr_terrain_field_cell_get(field, x, y)) continue;
            for(uint8_t dir = 0; dir < 4; dir++) {
                uint8_t nx = (uint8_t)(x + dirs[dir][0]);
                uint8_t ny = (uint8_t)(y + dirs[dir][1]);
                if(!fr_can_freeze_terrain(fr_get_terrain(game, nx, ny))) continue;
                fr_freeze_cell(game, field->floor, nx, ny);
                uint16_t index = (uint16_t)ny * FR_MAP_W + nx;
                next_cells[index / 8] |= (uint8_t)(1u << (index & 7u));
                spread = true;
            }
        }
    }
    memcpy(field->cells, next_cells, sizeof(field->cells));
    if(!spread) field->active = false;
}

static bool fr_slide_player_blocked(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_is_walkable(fr_get_terrain(game, x, y))) return true;
    if(fr_blocking_item_at(game, x, y)) return true;
    if(fr_actor_at(game, x, y)) return true;
    return false;
}

bool fr_try_ice_slide_player(FrGame* game, int8_t dx, int8_t dy) {
    if(dx == 0 && dy == 0) return false;
    if(fr_get_terrain(game, game->player.x, game->player.y) != FR_TERR_ICE) return false;
    if(fr_rand_u8(game, 100) >= 30) return false;
    bool moved = false;
    while(true) {
        int16_t nx_i = (int16_t)game->player.x + dx;
        int16_t ny_i = (int16_t)game->player.y + dy;
        if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) break;
        uint8_t nx = (uint8_t)nx_i;
        uint8_t ny = (uint8_t)ny_i;
        if(fr_get_terrain(game, nx, ny) != FR_TERR_ICE || fr_slide_player_blocked(game, nx, ny)) break;
        game->player.x = nx;
        game->player.y = ny;
        moved = true;
    }
    if(moved) fr_log(game, "Ice slide.");
    return moved;
}

static bool fr_slide_actor_blocked(FrGame* game, const FrActor* actor, uint8_t x, uint8_t y) {
    if(!fr_is_walkable(fr_get_terrain(game, x, y))) return true;
    if(fr_blocking_item_at(game, x, y)) return true;
    FrActor* blocker = fr_actor_at(game, x, y);
    if(blocker && blocker != actor) return true;
    if(game->player.x == x && game->player.y == y) return true;
    return false;
}

bool fr_try_ice_slide_actor(FrGame* game, FrActor* actor, int8_t dx, int8_t dy) {
    if(!actor || actor->type == FR_MON_EEL || (dx == 0 && dy == 0)) return false;
    if(fr_get_terrain(game, actor->x, actor->y) != FR_TERR_ICE) return false;
    if(fr_rand_u8(game, 100) >= 30) return false;
    bool moved = false;
    while(true) {
        int16_t nx_i = (int16_t)actor->x + dx;
        int16_t ny_i = (int16_t)actor->y + dy;
        if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) break;
        uint8_t nx = (uint8_t)nx_i;
        uint8_t ny = (uint8_t)ny_i;
        if(fr_get_terrain(game, nx, ny) != FR_TERR_ICE || fr_slide_actor_blocked(game, actor, nx, ny)) break;
        actor->x = nx;
        actor->y = ny;
        moved = true;
    }
    return moved;
}
