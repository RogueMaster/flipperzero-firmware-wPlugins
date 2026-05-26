#include "actor_state.h"

#include "game_core.h"
#include "monster_defs.h"

#include <stddef.h>
#include <string.h>

uint8_t fr_count_active_actors(const FrGame* game) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++)
        if(game->actors[i].active) count++;
    return count;
}

uint8_t fr_count_active_items(const FrGame* game) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++)
        if(game->items[i].active) count++;
    return count;
}

FrActor* fr_spawn_actor(FrGame* game, uint8_t type, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active) continue;
        FrActor* actor = &game->actors[i];
        memset(actor, 0, sizeof(*actor));
        actor->active = true;
        actor->type = type;
        actor->x = x;
        actor->y = y;
        actor->target_x = x;
        actor->target_y = y;
        const FrMonsterDef* def = fr_monster_def(type);
        actor->hp = actor->max_hp = def->hp;
        actor->dmg = def->damage;
        if(def->hidden) actor->flags |= FR_ACTOR_HIDDEN;
        if(type == FR_MON_YONDER_WARDEN) {
            actor->memory = 255;
        }
        uint8_t chase_chance = fr_monster_chase_chance(type);
        if(chase_chance == 100 || fr_rand_u8(game, 100) < chase_chance) {
            actor->flags |= FR_ACTOR_CHASES;
        }
        return actor;
    }
    return NULL;
}

FrActor* fr_actor_at(FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game->actors[i];
        if(actor->active && actor->x == x && actor->y == y) return actor;
    }
    return NULL;
}

static void fr_configure_trap_source(FrGame* game, FrTrap* trap) {
    trap->source_x = trap->x;
    trap->source_y = trap->y;
    trap->dir_x = 0;
    trap->dir_y = 0;
    if(trap->type != FR_TRAP_ARROW && trap->type != FR_TRAP_FIRE) return;

    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(uint8_t dir = 0; dir < 4; dir++) {
        int16_t sx = trap->x;
        int16_t sy = trap->y;
        for(uint8_t step = 0; step < 16; step++) {
            sx = (int16_t)(sx + dirs[dir][0]);
            sy = (int16_t)(sy + dirs[dir][1]);
            if(sx <= 0 || sy <= 0 || sx >= FR_MAP_W - 1 || sy >= FR_MAP_H - 1) break;
            uint8_t terrain = fr_get_terrain(game, (uint8_t)sx, (uint8_t)sy);
            if(terrain == FR_TERR_WALL) {
                trap->source_x = (uint8_t)sx;
                trap->source_y = (uint8_t)sy;
                trap->dir_x = (int8_t)-dirs[dir][0];
                trap->dir_y = (int8_t)-dirs[dir][1];
                return;
            }
            if(!fr_is_walkable(terrain)) break;
        }
    }
}

bool fr_place_trap(FrGame* game, uint8_t x, uint8_t y, uint8_t type) {
    for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
        if(game->traps[i].active) continue;
        game->traps[i].active = true;
        game->traps[i].x = x;
        game->traps[i].y = y;
        game->traps[i].type = type;
        game->traps[i].hidden = 1;
        fr_configure_trap_source(game, &game->traps[i]);
        return true;
    }
    return false;
}

FrTrap* fr_trap_at(FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
        if(game->traps[i].active && game->traps[i].x == x && game->traps[i].y == y) return &game->traps[i];
    }
    return NULL;
}
