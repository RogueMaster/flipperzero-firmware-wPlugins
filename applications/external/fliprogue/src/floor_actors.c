#include "floor_actors.h"

#include "actor_state.h"
#include "floor_state.h"
#include "game_core.h"
#include "map_state.h"
#include "placement.h"

static bool fr_current_floor_has_warden(const FrGame* game) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active && game->actors[i].type == FR_MON_YONDER_WARDEN) return true;
    }
    return false;
}

static bool fr_add_actor_to_floor_state(FrFloorState* floor, const FrActor* actor, uint8_t x, uint8_t y) {
    if(fr_floor_actor_at_except(floor, x, y, 0xFF)) {
        for(uint8_t radius = 1; radius < 4; radius++) {
            bool found = false;
            for(int8_t oy = -(int8_t)radius; oy <= (int8_t)radius && !found; oy++) {
                for(int8_t ox = -(int8_t)radius; ox <= (int8_t)radius && !found; ox++) {
                    int16_t nx = (int16_t)x + ox;
                    int16_t ny = (int16_t)y + oy;
                    if(nx <= 0 || ny <= 0 || nx >= FR_MAP_W - 1 || ny >= FR_MAP_H - 1) continue;
                    if(!fr_floor_actor_at_except(floor, (uint8_t)nx, (uint8_t)ny, 0xFF)) {
                        x = (uint8_t)nx;
                        y = (uint8_t)ny;
                        found = true;
                    }
                }
            }
            if(found) break;
        }
    }
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(floor->actors[i].active) continue;
        FrActor traveler = *actor;
        traveler.x = x;
        traveler.y = y;
        traveler.active = true;
        fr_saved_actor_from_actor(&floor->actors[i], &traveler);
        return true;
    }
    return false;
}

static bool fr_add_actor_to_current_floor(FrGame* game, const FrActor* actor, uint8_t x, uint8_t y) {
    if(!fr_find_free_near(game, &x, &y)) return false;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active) continue;
        game->actors[i] = *actor;
        game->actors[i].x = x;
        game->actors[i].y = y;
        game->actors[i].active = true;
        return true;
    }
    return false;
}

static bool fr_try_move_actor_floor(FrFloorState* floor, uint8_t actor_index, int8_t dx, int8_t dy) {
    FrSavedActor* actor = &floor->actors[actor_index];
    if(dx == 0 && dy == 0) return false;
    int16_t nx_i = (int16_t)actor->x + dx;
    int16_t ny_i = (int16_t)actor->y + dy;
    if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) return false;
    uint8_t nx = (uint8_t)nx_i;
    uint8_t ny = (uint8_t)ny_i;
    if(fr_floor_actor_at_except(floor, nx, ny, actor_index)) return false;
    actor->x = nx;
    actor->y = ny;
    return true;
}

void fr_warden_global_turn(FrGame* game) {
    if(!game->player.has_orb || fr_current_floor_has_warden(game)) return;

    for(uint8_t floor_id = 1; floor_id <= FR_MAX_FLOORS; floor_id++) {
        FrFloorState* floor = &game->floors[floor_id - 1];
        if(!floor->generated || floor_id == game->floor) continue;
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            FrSavedActor* actor = &floor->actors[i];
            if(!actor->active || actor->type != FR_MON_YONDER_WARDEN) continue;

            uint8_t entry_terrain = floor_id > game->floor ? FR_TERR_STAIRS_DOWN : FR_TERR_STAIRS_UP;
            uint8_t next_floor = floor_id > game->floor ? (uint8_t)(floor_id - 1) : (uint8_t)(floor_id + 1);
            uint8_t tx = floor_id > game->floor ? floor->up_x : floor->down_x;
            uint8_t ty = floor_id > game->floor ? floor->up_y : floor->down_y;
            actor->cooldown++;
            if(actor->cooldown < 8) {
                int8_t dx = fr_sign_i8((int16_t)tx - (int16_t)actor->x);
                int8_t dy = dx == 0 ? fr_sign_i8((int16_t)ty - (int16_t)actor->y) : 0;
                fr_try_move_actor_floor(floor, i, dx, dy);
                return;
            }

            if(next_floor >= 1 && next_floor <= FR_MAX_FLOORS) {
                FrActor traveler;
                fr_actor_from_saved_actor(&traveler, actor);
                actor->active = false;
                traveler.cooldown = 0;
                if(next_floor == game->floor) {
                    uint8_t ex = 0;
                    uint8_t ey = 0;
                    if(fr_find_first_tile(game, entry_terrain, &ex, &ey)) fr_add_actor_to_current_floor(game, &traveler, ex, ey);
                } else {
                    FrFloorState* next = &game->floors[next_floor - 1];
                    if(next->generated) {
                        uint8_t ex = entry_terrain == FR_TERR_STAIRS_UP ? next->up_x : next->down_x;
                        uint8_t ey = entry_terrain == FR_TERR_STAIRS_UP ? next->up_y : next->down_y;
                        fr_add_actor_to_floor_state(next, &traveler, ex, ey);
                    } else {
                        actor->active = true;
                    }
                }
            }
            return;
        }
    }
}
