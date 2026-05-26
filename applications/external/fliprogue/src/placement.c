#include "placement.h"

#include "actor_state.h"
#include "game_core.h"
#include "map_state.h"

#include <stddef.h>

bool fr_item_exists_at(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active && game->items[i].x == x && game->items[i].y == y) return true;
    }
    return false;
}

bool fr_blocking_item_at(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        const FrItem* item = &game->items[i];
        if(item->active && item->type == FR_ITEM_CHEST && (item->flags & FR_ITEM_FLAG_OPENED) == 0 &&
           item->x == x && item->y == y) {
            return true;
        }
    }
    return false;
}

bool fr_can_place_object(FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(!fr_is_walkable(terrain) || terrain == FR_TERR_DOOR_CLOSED || terrain == FR_TERR_STAIRS_DOWN ||
       terrain == FR_TERR_STAIRS_UP || terrain == FR_TERR_WATER) {
        return false;
    }
    if(game->player.x == x && game->player.y == y) return false;
    if(fr_actor_at(game, x, y) != NULL) return false;
    if(fr_item_exists_at(game, x, y)) return false;
    if(fr_trap_at(game, x, y) != NULL) return false;
    return true;
}

bool fr_find_free_near(FrGame* game, uint8_t* x, uint8_t* y) {
    for(uint8_t radius = 0; radius < 8; radius++) {
        for(int8_t oy = -(int8_t)radius; oy <= (int8_t)radius; oy++) {
            for(int8_t ox = -(int8_t)radius; ox <= (int8_t)radius; ox++) {
                int16_t nx = (int16_t)*x + ox;
                int16_t ny = (int16_t)*y + oy;
                if(nx <= 0 || ny <= 0 || nx >= FR_MAP_W - 1 || ny >= FR_MAP_H - 1) continue;
                uint8_t tx = (uint8_t)nx;
                uint8_t ty = (uint8_t)ny;
                if(fr_can_place_object(game, tx, ty)) {
                    *x = tx;
                    *y = ty;
                    return true;
                }
            }
        }
    }
    return false;
}

bool fr_find_floor_near(FrGame* game, uint8_t* x, uint8_t* y, uint8_t max_radius) {
    for(uint8_t radius = 0; radius <= max_radius; radius++) {
        for(int8_t oy = -(int8_t)radius; oy <= (int8_t)radius; oy++) {
            for(int8_t ox = -(int8_t)radius; ox <= (int8_t)radius; ox++) {
                int16_t nx = (int16_t)*x + ox;
                int16_t ny = (int16_t)*y + oy;
                if(nx <= 0 || ny <= 0 || nx >= FR_MAP_W - 1 || ny >= FR_MAP_H - 1) continue;
                if(fr_get_terrain(game, (uint8_t)nx, (uint8_t)ny) == FR_TERR_FLOOR) {
                    *x = (uint8_t)nx;
                    *y = (uint8_t)ny;
                    return true;
                }
            }
        }
    }
    return false;
}

bool fr_room_is_large_enough(const FrRoom* room, uint8_t min_w, uint8_t min_h) {
    return room != NULL && room->w >= min_w && room->h >= min_h;
}

bool fr_find_room_center_floor(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y) {
    if(!fr_room_is_large_enough(room, 3, 3)) return false;
    uint8_t cx = (uint8_t)(room->x + room->w / 2);
    uint8_t cy = (uint8_t)(room->y + room->h / 2);
    if(fr_get_terrain(game, cx, cy) == FR_TERR_FLOOR) {
        *x = cx;
        *y = cy;
        return true;
    }
    *x = cx;
    *y = cy;
    return fr_find_floor_near(game, x, y, 3);
}

bool fr_find_room_perimeter_floor(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y) {
    if(!fr_room_is_large_enough(room, 3, 3)) return false;
    for(uint8_t yy = room->y; yy < room->y + room->h; yy++) {
        for(uint8_t xx = room->x; xx < room->x + room->w; xx++) {
            bool edge = xx == room->x || yy == room->y || xx + 1 == room->x + room->w ||
                        yy + 1 == room->y + room->h;
            if(!edge) continue;
            if(fr_get_terrain(game, xx, yy) == FR_TERR_FLOOR) {
                *x = xx;
                *y = yy;
                return true;
            }
        }
    }
    return false;
}

bool fr_find_wall_opposed_floor_in_room(
    FrGame* game,
    const FrRoom* room,
    uint8_t* trigger_x,
    uint8_t* trigger_y,
    uint8_t* wall_x,
    uint8_t* wall_y) {
    if(!fr_room_is_large_enough(room, 4, 4)) return false;
    const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(uint8_t y = (uint8_t)(room->y + 1); y + 1 < room->y + room->h; y++) {
        for(uint8_t x = (uint8_t)(room->x + 1); x + 1 < room->x + room->w; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR) continue;
            for(uint8_t dir = 0; dir < 4; dir++) {
                int16_t wx_i = (int16_t)x + dirs[dir][0];
                int16_t wy_i = (int16_t)y + dirs[dir][1];
                while(wx_i > 0 && wy_i > 0 && wx_i < FR_MAP_W - 1 && wy_i < FR_MAP_H - 1) {
                    uint8_t terrain = fr_get_terrain(game, (uint8_t)wx_i, (uint8_t)wy_i);
                    if(terrain == FR_TERR_WALL) {
                        *trigger_x = x;
                        *trigger_y = y;
                        *wall_x = (uint8_t)wx_i;
                        *wall_y = (uint8_t)wy_i;
                        return true;
                    }
                    if(terrain != FR_TERR_FLOOR && terrain != FR_TERR_GRASS && terrain != FR_TERR_PUDDLE) break;
                    wx_i = (int16_t)(wx_i + dirs[dir][0]);
                    wy_i = (int16_t)(wy_i + dirs[dir][1]);
                }
            }
        }
    }
    return false;
}

bool fr_find_secret_wall_candidate_for_room(FrGame* game, const FrRoom* room, uint8_t* x, uint8_t* y) {
    if(!fr_room_is_large_enough(room, 4, 4)) return false;
    uint8_t candidates[4][2] = {
        {(uint8_t)(room->x - 1), (uint8_t)(room->y + room->h / 2)},
        {(uint8_t)(room->x + room->w), (uint8_t)(room->y + room->h / 2)},
        {(uint8_t)(room->x + room->w / 2), (uint8_t)(room->y - 1)},
        {(uint8_t)(room->x + room->w / 2), (uint8_t)(room->y + room->h)},
    };
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t cx = candidates[i][0];
        uint8_t cy = candidates[i][1];
        if(!fr_in_bounds(cx, cy)) continue;
        if(fr_get_terrain(game, cx, cy) == FR_TERR_WALL && (game->tiles[cy][cx] & FR_TILE_HIDDEN_DOOR) == 0) {
            *x = cx;
            *y = cy;
            return true;
        }
    }
    return false;
}
