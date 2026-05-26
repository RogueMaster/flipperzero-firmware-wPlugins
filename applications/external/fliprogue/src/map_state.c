#include "map_state.h"

#include "game_core.h"

uint8_t fr_tile_core(const FrGame* game, uint8_t x, uint8_t y) {
    return game->tiles[y][x] & (FR_TILE_TERRAIN_MASK | FR_TILE_HIDDEN_DOOR);
}

void fr_record_tile_delta(FrGame* game, uint8_t x, uint8_t y) {
    if(game->suppress_deltas || game->floor == 0 || game->floor > FR_MAX_FLOORS) return;
    FrFloorState* floor = &game->floors[game->floor - 1];
    if(!floor->generated) return;
    uint16_t pos = (uint16_t)y * FR_MAP_W + x;
    uint8_t tile = fr_tile_core(game, x, y);
    for(uint8_t i = 0; i < floor->tile_delta_count; i++) {
        if(floor->tile_delta_pos[i] == pos) {
            floor->tile_delta_tile[i] = tile;
            return;
        }
    }
    if(floor->tile_delta_count < FR_MAX_TILE_DELTAS) {
        floor->tile_delta_pos[floor->tile_delta_count] = pos;
        floor->tile_delta_tile[floor->tile_delta_count] = tile;
        floor->tile_delta_count++;
    }
}

uint8_t fr_get_terrain(const FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_in_bounds(x, y)) return FR_TERR_VOID;
    return game->tiles[y][x] & FR_TILE_TERRAIN_MASK;
}

void fr_set_terrain(FrGame* game, uint8_t x, uint8_t y, uint8_t terrain) {
    if(!fr_in_bounds(x, y)) return;
    game->tiles[y][x] = (uint8_t)((game->tiles[y][x] & ~FR_TILE_TERRAIN_MASK) | (terrain & FR_TILE_TERRAIN_MASK));
    fr_record_tile_delta(game, x, y);
}

void fr_mark_hidden_door(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_in_bounds(x, y)) return;
    if(fr_get_terrain(game, x, y) == FR_TERR_WALL) game->tiles[y][x] |= FR_TILE_HIDDEN_DOOR;
}

bool fr_reveal_hidden_door_at(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_in_bounds(x, y) || (game->tiles[y][x] & FR_TILE_HIDDEN_DOOR) == 0) return false;
    game->tiles[y][x] &= (uint8_t)~FR_TILE_HIDDEN_DOOR;
    fr_set_terrain(game, x, y, FR_TERR_DOOR_CLOSED);
    return true;
}

bool fr_find_first_tile(const FrGame* game, uint8_t terrain, uint8_t* out_x, uint8_t* out_y) {
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if(fr_get_terrain(game, x, y) == terrain) {
                if(out_x) *out_x = x;
                if(out_y) *out_y = y;
                return true;
            }
        }
    }
    return false;
}
