#include "monster_reactions.h"

#include "actor_state.h"
#include "game_core.h"
#include "map_state.h"
#include "monster_defs.h"
#include "status_effects.h"

void fr_wake_actor_toward_player(FrGame* game, FrActor* actor) {
    actor->flags &= (uint8_t)~FR_ACTOR_ASLEEP;
    actor->target_x = game->player.x;
    actor->target_y = game->player.y;
    actor->target_dx = fr_sign_i8((int16_t)game->player.x - (int16_t)actor->x);
    actor->target_dy = fr_sign_i8((int16_t)game->player.y - (int16_t)actor->y);
    if(actor->type == FR_MON_YONDER_WARDEN) actor->memory = 255;
    else if(actor->memory < 20) actor->memory = 20;
}

void fr_combat_wake_actor(FrGame* game, FrActor* actor) {
    fr_wake_actor_toward_player(game, actor);
    actor->flags |= FR_ACTOR_CHASES;
}

void fr_pack_wake(FrGame* game, const FrActor* source) {
    if(!source || source->pack_id == 0) return;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game->actors[i];
        if(!actor->active || actor->pack_id != source->pack_id) continue;
        fr_wake_actor_toward_player(game, actor);
    }
}

void fr_pack_combat_wake(FrGame* game, const FrActor* source) {
    if(!source || source->pack_id == 0) return;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game->actors[i];
        if(!actor->active || actor->pack_id != source->pack_id) continue;
        fr_combat_wake_actor(game, actor);
    }
}

uint8_t fr_active_pack_count(const FrGame* game, uint8_t pack_id) {
    if(pack_id == 0) return 1;
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        const FrActor* actor = &game->actors[i];
        if(actor->active && actor->pack_id == pack_id) count++;
    }
    return count;
}

static uint8_t fr_new_pack_id(FrGame* game) {
    uint8_t pack_id = 0;
    while(pack_id == 0) pack_id = (uint8_t)(1 + fr_rand_u8(game, 220));
    return pack_id;
}

static bool fr_tile_free_for_split(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_in_bounds(x, y)) return false;
    if(game->player.x == x && game->player.y == y) return false;
    if(fr_actor_at(game, x, y)) return false;
    return fr_is_walkable(fr_get_terrain(game, x, y));
}

static bool fr_find_slime_split_tile(FrGame* game, const FrActor* actor, uint8_t* out_x, uint8_t* out_y) {
    static const int8_t spots[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    uint8_t start = fr_rand_u8(game, 8);
    for(uint8_t i = 0; i < 8; i++) {
        uint8_t pick = (uint8_t)((start + i) % 8);
        int16_t nx_i = (int16_t)game->player.x + spots[pick][0];
        int16_t ny_i = (int16_t)game->player.y + spots[pick][1];
        if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
        uint8_t nx = (uint8_t)nx_i;
        uint8_t ny = (uint8_t)ny_i;
        if(fr_tile_free_for_split(game, nx, ny)) {
            *out_x = nx;
            *out_y = ny;
            return true;
        }
    }
    for(uint8_t radius = 1; radius <= 3; radius++) {
        for(int8_t oy = -(int8_t)radius; oy <= (int8_t)radius; oy++) {
            for(int8_t ox = -(int8_t)radius; ox <= (int8_t)radius; ox++) {
                if(fr_abs_i8(ox) != radius && fr_abs_i8(oy) != radius) continue;
                int16_t nx_i = (int16_t)actor->x + ox;
                int16_t ny_i = (int16_t)actor->y + oy;
                if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
                uint8_t nx = (uint8_t)nx_i;
                uint8_t ny = (uint8_t)ny_i;
                if(fr_tile_free_for_split(game, nx, ny)) {
                    *out_x = nx;
                    *out_y = ny;
                    return true;
                }
            }
        }
    }
    return false;
}

bool fr_maybe_split_slime(FrGame* game, FrActor* actor, uint8_t kind) {
    if(actor->type != FR_MON_SLIME || kind == FR_DAMAGE_BURST || kind == FR_DAMAGE_DOT || actor->hp < 2) return false;
    uint8_t sx = 0;
    uint8_t sy = 0;
    if(!fr_find_slime_split_tile(game, actor, &sx, &sy)) return false;
    if(actor->pack_id == 0) actor->pack_id = fr_new_pack_id(game);
    uint8_t half_hp = (uint8_t)(actor->hp / 2u);
    if(half_hp == 0) half_hp = 1;
    actor->hp = half_hp;
    FrActor* split = fr_spawn_actor(game, FR_MON_SLIME, sx, sy);
    if(!split) return false;
    split->hp = half_hp;
    split->max_hp = actor->max_hp;
    split->pack_id = actor->pack_id;
    fr_combat_wake_actor(game, split);
    fr_log(game, "Slime splits.");
    return true;
}

void fr_maybe_actor_breaks(FrGame* game, FrActor* actor) {
    if(!actor->active || actor->type == FR_MON_YONDER_WARDEN || (actor->effects & FR_FX_AFRAID) != 0) return;
    if((uint16_t)actor->hp * 10u >= (uint16_t)actor->max_hp * 3u) return;
    if(fr_active_pack_count(game, actor->pack_id) > 1) return;
    if(fr_rand_u8(game, 100) >= 15) return;
    fr_apply_effect_to_actor(actor, FR_FX_AFRAID, FR_FX_AFRAID_INDEX, 6);
    fr_log(game, "%s breaks.", fr_actor_log_name(actor->type));
}

bool fr_actor_evades_player_hit(FrGame* game, FrActor* actor, uint8_t kind) {
    if(actor->type == FR_MON_ARCHER &&
       (kind == FR_DAMAGE_PROJECTILE || kind == FR_DAMAGE_THROWN) &&
       fr_rand_u8(game, 100) < 50) {
        fr_combat_wake_actor(game, actor);
        fr_pack_combat_wake(game, actor);
        fr_log(game, kind == FR_DAMAGE_MELEE ? "You miss %s." : "Shot misses %s.", fr_actor_log_name(actor->type));
        return true;
    }
    if(actor->type != FR_MON_RAT) return false;
    if(fr_rand_u8(game, 100) >= 35) return false;
    fr_combat_wake_actor(game, actor);
    fr_pack_combat_wake(game, actor);
    fr_log(game, kind == FR_DAMAGE_MELEE ? "You miss %s." : "Shot misses %s.", fr_actor_log_name(actor->type));
    return true;
}

static bool fr_actor_can_sleep(uint8_t type) {
    return type == FR_MON_RAT || type == FR_MON_BAT || type == FR_MON_SNAKE;
}

void fr_maybe_actor_starts_asleep(FrGame* game, FrActor* actor) {
    if(!actor || actor->type == FR_MON_YONDER_WARDEN) return;
    uint8_t roll = fr_rand_u8(game, 100);
    if(fr_actor_can_sleep(actor->type) && roll < 60) {
        actor->flags |= FR_ACTOR_ASLEEP;
    } else if(roll >= 90) {
        actor->flags |= FR_ACTOR_ROAMS;
    }
    actor->target_x = actor->x;
    actor->target_y = actor->y;
}
