#include "combat.h"

#include "actor_state.h"
#include "equipment.h"
#include "game_core.h"
#include "map_state.h"
#include "monster_defs.h"
#include "monster_reactions.h"
#include "placement.h"
#include "status_effects.h"

void fr_damage_actor_kind(FrGame* game, FrActor* actor, uint8_t damage, const char* verb, uint8_t kind) {
    if(!actor) return;
    fr_reveal_actor(actor);
    if(fr_actor_evades_player_hit(game, actor, kind)) return;
    if((actor->effects & FR_FX_MARKED) != 0 && damage < 254) damage++;
    if(actor->hp > damage) {
        actor->hp = (uint8_t)(actor->hp - damage);
        fr_combat_wake_actor(game, actor);
        fr_pack_combat_wake(game, actor);
        fr_log(game, "%s %s.", verb, fr_actor_log_name(actor->type));
        if(!fr_maybe_split_slime(game, actor, kind)) fr_maybe_actor_breaks(game, actor);
    } else {
        fr_kill_actor(game, actor);
        fr_log(game, "%s dies.", fr_actor_log_name(actor->type));
    }
}

void fr_damage_actor(FrGame* game, FrActor* actor, uint8_t damage, const char* verb) {
    fr_damage_actor_kind(game, actor, damage, verb, FR_DAMAGE_MELEE);
}

bool fr_force_push_actor(FrGame* game, FrActor* actor, int8_t dx, int8_t dy) {
    if(!actor || (dx == 0 && dy == 0)) return false;
    int16_t nx_i = (int16_t)actor->x + dx;
    int16_t ny_i = (int16_t)actor->y + dy;
    if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) {
        fr_damage_actor_kind(game, actor, 1, "Wall hurts", FR_DAMAGE_BURST);
        return false;
    }
    uint8_t nx = (uint8_t)nx_i;
    uint8_t ny = (uint8_t)ny_i;
    if(!fr_is_walkable(fr_get_terrain(game, nx, ny)) || fr_actor_at(game, nx, ny) ||
       fr_blocking_item_at(game, nx, ny) ||
       (game->player.x == nx && game->player.y == ny)) {
        fr_damage_actor_kind(game, actor, 1, "Wall hurts", FR_DAMAGE_BURST);
        return false;
    }
    actor->x = nx;
    actor->y = ny;
    fr_log(game, "Pushed.");
    return true;
}

void fr_actor_hit_player_effects(FrGame* game, FrActor* actor) {
    fr_reveal_actor(actor);
    if(actor->type == FR_MON_SNAKE && fr_rand_u8(game, 100) < 5) {
        fr_apply_effect_to_player(&game->player, FR_FX_POISONED, FR_FX_POISONED_INDEX, 8);
        fr_log(game, "Venom sinks.");
    } else if(actor->type == FR_MON_BAT && actor->hp < actor->max_hp && fr_rand_u8(game, 100) < 10) {
        actor->hp++;
        fr_log(game, "Bat sips.");
    } else if(actor->type == FR_MON_WIGHT && fr_rand_u8(game, 100) < 5) {
        fr_apply_effect_to_player(&game->player, FR_FX_AFRAID, FR_FX_AFRAID_INDEX, 6);
        fr_log(game, "Dread rises.");
    } else if(actor->type == FR_MON_WISP && fr_rand_u8(game, 100) < 15) {
        fr_apply_effect_to_player(&game->player, FR_FX_BLIND, FR_FX_BLIND_INDEX, 5);
        fr_log(game, "Light lies.");
    } else if(actor->type == FR_MON_OGRE && fr_rand_u8(game, 100) < 15) {
        fr_apply_effect_to_player(&game->player, FR_FX_STUNNED, FR_FX_STUNNED_INDEX, 2);
        fr_log(game, "Ogre stuns.");
    }
    if(fr_has_equipped_trinket(game, FR_TRINKET_FANG) && fr_rand_u8(game, 100) < 5) {
        fr_apply_effect_to_actor(actor, FR_FX_AFRAID, FR_FX_AFRAID_INDEX, 5);
        fr_log(game, "Fang scares.");
    }
}

void fr_cube_tick(FrGame* game) {
    if(game->player.cube_hp == 0 || game->mode != FR_MODE_PLAYING) return;
    uint8_t dmg = (uint8_t)(2 + fr_rand_u8(game, 2));
    if(game->player.hp > dmg) {
        game->player.hp = (uint8_t)(game->player.hp - dmg);
        fr_log(game, "Still in Cube.");
    } else {
        game->player.hp = 0;
        fr_set_game_over(game, FR_DEATH_KILLED, "Cube digests you.");
    }
}

void fr_hit_cube_from_inside(FrGame* game, uint8_t damage) {
    if(game->player.cube_hp == 0) return;
    if(game->player.cube_hp > damage) {
        game->player.cube_hp = (uint8_t)(game->player.cube_hp - damage);
        fr_log(game, "You thrash Cube.");
    } else {
        game->player.cube_hp = 0;
        game->player.cube_max_hp = 0;
        fr_log(game, "Cube bursts.");
    }
}
