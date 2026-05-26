#include "targeting.h"

#include "actor_state.h"
#include "game_core.h"
#include "placement.h"

#include <stddef.h>

FrActor* fr_line_actor(FrGame* game, int8_t dx, int8_t dy) {
    uint8_t x = game->player.x;
    uint8_t y = game->player.y;
    for(uint8_t step = 0; step < 12; step++) {
        int16_t nx = (int16_t)x + dx;
        int16_t ny = (int16_t)y + dy;
        if(nx < 0 || ny < 0 || nx >= FR_MAP_W || ny >= FR_MAP_H) return NULL;
        x = (uint8_t)nx;
        y = (uint8_t)ny;
        if(fr_blocks_sight(fr_get_terrain(game, x, y))) return NULL;
        FrActor* actor = fr_actor_at(game, x, y);
        if(actor) return fr_actor_visible_to_player(game, actor) ? actor : NULL;
    }
    return NULL;
}

bool fr_fear_direction(const FrGame* game, int8_t* dx, int8_t* dy) {
    uint8_t best = 0xFF;
    int8_t best_x = 0;
    int8_t best_y = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        const FrActor* actor = &game->actors[i];
        if(!actor->active) continue;
        int8_t diff_x = (int8_t)actor->x - (int8_t)game->player.x;
        int8_t diff_y = (int8_t)actor->y - (int8_t)game->player.y;
        uint8_t dist = (uint8_t)(fr_abs_i8(diff_x) + fr_abs_i8(diff_y));
        if(dist == 0 || dist > 8 || dist >= best) continue;
        best = dist;
        best_x = diff_x;
        best_y = diff_y;
    }
    if(best == 0xFF) return false;
    if(fr_abs_i8(best_x) >= fr_abs_i8(best_y) && best_x != 0) {
        *dx = best_x > 0 ? -1 : 1;
        *dy = 0;
    } else {
        *dx = 0;
        *dy = best_y > 0 ? -1 : 1;
    }
    return true;
}

bool fr_resolve_blink_destination(FrGame* game, uint8_t tx, uint8_t ty, uint8_t* out_x, uint8_t* out_y) {
    int16_t x = game->player.x;
    int16_t y = game->player.y;
    int16_t target_x = tx;
    int16_t target_y = ty;
    if(x == target_x && y == target_y) return false;

    int16_t dx = target_x > x ? (int16_t)(target_x - x) : (int16_t)(x - target_x);
    int16_t dy = target_y > y ? (int16_t)(target_y - y) : (int16_t)(y - target_y);
    int16_t sx = x < target_x ? 1 : -1;
    int16_t sy = y < target_y ? 1 : -1;
    int16_t err = (int16_t)(dx - dy);
    uint8_t best_x = game->player.x;
    uint8_t best_y = game->player.y;
    bool found = false;

    while(x != target_x || y != target_y) {
        int16_t e2 = (int16_t)(err * 2);
        if(e2 > -dy) {
            err = (int16_t)(err - dy);
            x = (int16_t)(x + sx);
        }
        if(e2 < dx) {
            err = (int16_t)(err + dx);
            y = (int16_t)(y + sy);
        }
        if(x < 0 || y < 0 || x >= FR_MAP_W || y >= FR_MAP_H) break;

        uint8_t ux = (uint8_t)x;
        uint8_t uy = (uint8_t)y;
        if(!fr_is_walkable(fr_get_terrain(game, ux, uy)) || fr_blocking_item_at(game, ux, uy)) break;
        if(fr_actor_at(game, ux, uy) == NULL) {
            best_x = ux;
            best_y = uy;
            found = true;
        }
    }

    if(!found) return false;
    *out_x = best_x;
    *out_y = best_y;
    return best_x != game->player.x || best_y != game->player.y;
}
