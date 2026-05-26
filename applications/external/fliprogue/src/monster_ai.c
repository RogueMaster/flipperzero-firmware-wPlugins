#include "monster_ai.h"

#include "actor_state.h"
#include "combat.h"
#include "game_core.h"
#include "hazards.h"
#include "ice_effects.h"
#include "map_state.h"
#include "monster_defs.h"
#include "monster_reactions.h"
#include "pathing.h"
#include "placement.h"
#include "status_effects.h"

#include <stddef.h>

static bool fr_actor_can_enter_terrain(const FrActor* actor, uint8_t terrain) {
    if(actor->type == FR_MON_EEL) {
        return terrain == FR_TERR_WATER || terrain == FR_TERR_PUDDLE;
    }
    if(terrain == FR_TERR_WATER) return false;
    return fr_is_walkable(terrain);
}

static bool fr_player_is_waterborne(const FrGame* game) {
    uint8_t terrain = fr_get_terrain(game, game->player.x, game->player.y);
    return terrain == FR_TERR_WATER || terrain == FR_TERR_PUDDLE;
}

bool fr_try_move_actor_current(FrGame* game, FrActor* actor, int8_t dx, int8_t dy) {
    if(dx == 0 && dy == 0) return false;
    int16_t nx_i = (int16_t)actor->x + dx;
    int16_t ny_i = (int16_t)actor->y + dy;
    if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) return false;
    uint8_t nx = (uint8_t)nx_i;
    uint8_t ny = (uint8_t)ny_i;
    if(nx == game->player.x && ny == game->player.y) return false;
    if(fr_actor_at(game, nx, ny) != NULL || fr_blocking_item_at(game, nx, ny)) return false;
    uint8_t terrain = fr_get_terrain(game, nx, ny);
    if(terrain == FR_TERR_DOOR_CLOSED && actor->type != FR_MON_EEL) {
        fr_set_terrain(game, nx, ny, FR_TERR_DOOR_OPEN);
        terrain = FR_TERR_DOOR_OPEN;
    }
    if(!fr_actor_can_enter_terrain(actor, terrain)) return false;
    actor->x = nx;
    actor->y = ny;
    if(terrain == FR_TERR_ICE) fr_try_ice_slide_actor(game, actor, dx, dy);
    return true;
}

static bool fr_find_free_around_player(FrGame* game, uint8_t* out_x, uint8_t* out_y, uint8_t start) {
    static const int8_t spots[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    for(uint8_t i = 0; i < 8; i++) {
        uint8_t pick = (uint8_t)((start + i) % 8);
        int16_t nx_i = (int16_t)game->player.x + spots[pick][0];
        int16_t ny_i = (int16_t)game->player.y + spots[pick][1];
        if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
        uint8_t nx = (uint8_t)nx_i;
        uint8_t ny = (uint8_t)ny_i;
        if(!fr_is_walkable(fr_get_terrain(game, nx, ny))) continue;
        if(fr_actor_at(game, nx, ny) || fr_blocking_item_at(game, nx, ny)) continue;
        *out_x = nx;
        *out_y = ny;
        return true;
    }
    return false;
}

static bool fr_bat_blink_strike(FrGame* game, FrActor* actor, uint8_t manhattan) {
    if(actor->type != FR_MON_BAT) return false;
    if(manhattan == 2 || (manhattan == 1 && fr_rand_u8(game, 100) < 35)) {
        uint8_t nx = 0;
        uint8_t ny = 0;
        if(fr_find_free_around_player(game, &nx, &ny, fr_rand_u8(game, 8))) {
            actor->x = nx;
            actor->y = ny;
            fr_log(game, "Bat blinks.");
            return true;
        }
    }
    return false;
}

static bool fr_cube_engulfs(FrGame* game, FrActor* actor, uint8_t manhattan) {
    if(actor->type != FR_MON_CUBE || manhattan != 1 || game->player.cube_hp > 0) return false;
    if(actor->hp != actor->max_hp || fr_rand_u8(game, 100) >= 20) return false;
    game->player.cube_hp = (uint8_t)(actor->max_hp * 2);
    game->player.cube_max_hp = game->player.cube_hp;
    actor->active = false;
    fr_log(game, "Cube engulfs you.");
    return true;
}

static bool fr_actor_wakes_near_player(FrGame* game, FrActor* actor, uint8_t manhattan) {
    if((actor->flags & FR_ACTOR_ASLEEP) == 0 || manhattan > 6) return false;
    uint8_t chance = manhattan <= 1 ? 100 : (uint8_t)(70 - manhattan * 10);
    if(fr_rand_u8(game, 100) >= chance) return false;
    if(manhattan <= 4 || (actor->flags & FR_ACTOR_CHASES) != 0) {
        fr_wake_actor_toward_player(game, actor);
    } else {
        actor->flags &= (uint8_t)~FR_ACTOR_ASLEEP;
    }
    return true;
}

static bool fr_actor_idle_tremble(FrGame* game, FrActor* actor) {
    if(fr_rand_u8(game, 100) >= 30) return false;
    if(actor->target_x == 0 && actor->target_y == 0) {
        actor->target_x = actor->x;
        actor->target_y = actor->y;
    }
    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    uint8_t start = fr_rand_u8(game, 4);
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t pick = (uint8_t)((start + i) % 4);
        int16_t nx_i = (int16_t)actor->x + dirs[pick][0];
        int16_t ny_i = (int16_t)actor->y + dirs[pick][1];
        if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
        uint8_t dist = (uint8_t)(fr_abs_i8((int8_t)nx_i - (int8_t)actor->target_x) +
                                 fr_abs_i8((int8_t)ny_i - (int8_t)actor->target_y));
        if(dist <= 1 && fr_try_move_actor_current(game, actor, dirs[pick][0], dirs[pick][1])) return true;
    }
    return false;
}

static void fr_pack_assign_roam_target(FrGame* game, const FrActor* source, uint8_t tx, uint8_t ty) {
    if(source->pack_id == 0) return;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game->actors[i];
        if(!actor->active || actor->pack_id != source->pack_id || (actor->flags & FR_ACTOR_ASLEEP) != 0) continue;
        actor->flags |= FR_ACTOR_ROAMS;
        actor->target_x = tx;
        actor->target_y = ty;
    }
}

static bool fr_actor_roam(FrGame* game, FrActor* actor) {
    uint8_t target_dist = (uint8_t)(fr_abs_i8((int8_t)actor->target_x - (int8_t)actor->x) +
                                    fr_abs_i8((int8_t)actor->target_y - (int8_t)actor->y));
    if(actor->target_x == 0 || actor->target_y == 0 || target_dist <= 2) {
        uint8_t tx = actor->x;
        uint8_t ty = actor->y;
        if(!fr_random_reachable_tile(game, &tx, &ty, 40)) return fr_actor_idle_tremble(game, actor);
        actor->target_x = tx;
        actor->target_y = ty;
        fr_pack_assign_roam_target(game, actor, tx, ty);
    }

    int8_t dx = 0;
    int8_t dy = 0;
    if(!fr_find_path_step_current(game, actor, actor->target_x, actor->target_y, &dx, &dy)) {
        int8_t diff_x = (int8_t)actor->target_x - (int8_t)actor->x;
        int8_t diff_y = (int8_t)actor->target_y - (int8_t)actor->y;
        if(fr_abs_i8(diff_x) > fr_abs_i8(diff_y)) dx = fr_sign_i8(diff_x);
        else dy = fr_sign_i8(diff_y);
    }
    return fr_try_move_actor_current(game, actor, dx, dy);
}

static bool fr_actor_notice_player(FrGame* game, FrActor* actor, uint8_t manhattan) {
    if(manhattan == 0 || manhattan > 5 || (actor->effects & FR_FX_BLIND) != 0) return false;
    uint8_t chance = (uint8_t)(10 + (6 - manhattan) * 6);
    if((actor->flags & FR_ACTOR_CHASES) == 0) chance = (uint8_t)(chance / 2);
    if(fr_rand_u8(game, 100) >= chance) return false;
    fr_wake_actor_toward_player(game, actor);
    return true;
}

static bool fr_actor_has_line_to_player(const FrGame* game, const FrActor* actor) {
    if(actor->x != game->player.x && actor->y != game->player.y) return false;
    int8_t dx = fr_sign_i8((int16_t)game->player.x - (int16_t)actor->x);
    int8_t dy = fr_sign_i8((int16_t)game->player.y - (int16_t)actor->y);
    uint8_t x = actor->x;
    uint8_t y = actor->y;
    for(uint8_t step = 0; step < 8; step++) {
        x = (uint8_t)((int16_t)x + dx);
        y = (uint8_t)((int16_t)y + dy);
        if(x == game->player.x && y == game->player.y) return true;
        if(fr_blocks_sight(fr_get_terrain(game, x, y))) return false;
        if(fr_actor_at((FrGame*)game, x, y) != NULL) return false;
    }
    return false;
}

static bool fr_archer_retreats(FrGame* game, FrActor* actor, int8_t diff_x, int8_t diff_y) {
    int8_t dx = 0;
    int8_t dy = 0;
    if(fr_abs_i8(diff_x) >= fr_abs_i8(diff_y) && diff_x != 0) dx = (int8_t)-fr_sign_i8(diff_x);
    else if(diff_y != 0) dy = (int8_t)-fr_sign_i8(diff_y);
    if(fr_try_move_actor_current(game, actor, dx, dy)) return true;
    if(dx != 0) {
        if(fr_try_move_actor_current(game, actor, 0, 1)) return true;
        if(fr_try_move_actor_current(game, actor, 0, -1)) return true;
    } else {
        if(fr_try_move_actor_current(game, actor, 1, 0)) return true;
        if(fr_try_move_actor_current(game, actor, -1, 0)) return true;
    }
    return false;
}

static bool fr_archer_ranged_turn(FrGame* game, FrActor* actor, uint8_t manhattan, int8_t diff_x, int8_t diff_y) {
    if(actor->type != FR_MON_ARCHER || (actor->effects & (FR_FX_BLIND | FR_FX_AFRAID | FR_FX_CONFUSED)) != 0) return false;
    if(manhattan == 1) {
        if(fr_rand_u8(game, 100) < 50 && fr_archer_retreats(game, actor, diff_x, diff_y)) return true;
        return false;
    }
    if(manhattan < 2 || !fr_actor_has_line_to_player(game, actor)) return false;
    uint8_t block = fr_player_block(game);
    uint8_t dmg = actor->dmg > block ? (uint8_t)(actor->dmg - block) : 1;
    fr_event_projectile(game, actor->x, actor->y, game->player.x, game->player.y, '-');
    if(game->player.hp > dmg) {
        game->player.hp = (uint8_t)(game->player.hp - dmg);
        fr_log(game, "%s shoots you.", fr_actor_log_name(actor->type));
    } else {
        game->player.hp = 0;
        fr_reveal_actor(actor);
        fr_set_game_over(game, FR_DEATH_KILLED, "Killed by %s.", fr_actor_log_name(actor->type));
    }
    return true;
}

void fr_actor_turns(FrGame* game) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->mode != FR_MODE_PLAYING || game->player.hp == 0) return;
        FrActor* actor = &game->actors[i];
        if(!actor->active) continue;
        if((actor->effects & FR_FX_STUNNED) != 0) continue;
        if((actor->effects & FR_FX_SLOWED) != 0 && (game->turn & 1u) != 0) continue;

        int8_t diff_x = (int8_t)game->player.x - (int8_t)actor->x;
        int8_t diff_y = (int8_t)game->player.y - (int8_t)actor->y;
        uint8_t manhattan = (uint8_t)(fr_abs_i8(diff_x) + fr_abs_i8(diff_y));
        if(actor->type == FR_MON_YONDER_WARDEN && manhattan <= 6 && (game->turn & 1u) != 0) continue;
        if(fr_actor_wakes_near_player(game, actor, manhattan)) continue;
        if((actor->flags & FR_ACTOR_ASLEEP) != 0) continue;

        bool afraid = (actor->effects & FR_FX_AFRAID) != 0;
        if(!afraid && fr_cube_engulfs(game, actor, manhattan)) continue;
        if(!afraid && fr_bat_blink_strike(game, actor, manhattan)) {
            diff_x = (int8_t)game->player.x - (int8_t)actor->x;
            diff_y = (int8_t)game->player.y - (int8_t)actor->y;
            manhattan = (uint8_t)(fr_abs_i8(diff_x) + fr_abs_i8(diff_y));
            if(fr_abs_i8(diff_x) <= 1 && fr_abs_i8(diff_y) <= 1 && (diff_x != 0 || diff_y != 0)) {
                manhattan = 1;
            }
        }
        if(!afraid && actor->type == FR_MON_DRAGON && manhattan <= 3 &&
           (diff_x == 0 || diff_y == 0) && fr_rand_u8(game, 100) < 25) {
            fr_fire_burst(game, game->player.x, game->player.y);
            continue;
        }
        if(!afraid && fr_archer_ranged_turn(game, actor, manhattan, diff_x, diff_y)) continue;
        if(manhattan == 1 && !afraid && (actor->type != FR_MON_EEL || fr_player_is_waterborne(game))) {
            if((actor->flags & FR_ACTOR_HIDDEN) != 0) {
                fr_reveal_actor(actor);
                fr_log(game, "A shape lunges.");
            }
            uint8_t block = fr_player_block(game);
            uint8_t dmg = actor->dmg > block ? (uint8_t)(actor->dmg - block) : 1;
            if(game->player.hp > dmg) {
                game->player.hp = (uint8_t)(game->player.hp - dmg);
                fr_log(game, "%s hits you.", fr_actor_log_name(actor->type));
                fr_actor_hit_player_effects(game, actor);
                if(game->player.class_id == FR_CLASS_WARRIOR && (game->player.perks & FR_PERK_3) != 0 &&
                   actor->active && fr_rand_u8(game, 100) < 10) {
                    fr_damage_actor_kind(game, actor, 1, "Riposte hits", FR_DAMAGE_MELEE);
                }
            } else {
                game->player.hp = 0;
                fr_reveal_actor(actor);
                fr_set_game_over(game, FR_DEATH_KILLED, "Killed by %s.", fr_actor_log_name(actor->type));
            }
            continue;
        }

        if(actor->type == FR_MON_YONDER_WARDEN) {
            actor->target_x = game->player.x;
            actor->target_y = game->player.y;
            actor->target_dx = fr_sign_i8(diff_x);
            actor->target_dy = fr_sign_i8(diff_y);
            actor->memory = 255;
        } else if((actor->flags & FR_ACTOR_CHASES) != 0 && manhattan <= 8 && (actor->effects & FR_FX_BLIND) == 0) {
            if(actor->memory > 0) {
                actor->target_dx = fr_sign_i8((int16_t)game->player.x - (int16_t)actor->target_x);
                actor->target_dy = fr_sign_i8((int16_t)game->player.y - (int16_t)actor->target_y);
            } else {
                actor->target_dx = fr_sign_i8(diff_x);
                actor->target_dy = fr_sign_i8(diff_y);
            }
            actor->target_x = game->player.x;
            actor->target_y = game->player.y;
            actor->memory = 20;
            fr_pack_wake(game, actor);
        } else if(actor->memory > 0) {
            actor->memory--;
            diff_x = (int8_t)actor->target_x - (int8_t)actor->x;
            diff_y = (int8_t)actor->target_y - (int8_t)actor->y;
            if(diff_x == 0 && diff_y == 0) {
                int16_t next_tx = (int16_t)actor->target_x + actor->target_dx;
                int16_t next_ty = (int16_t)actor->target_y + actor->target_dy;
                if(next_tx > 0 && next_ty > 0 && next_tx < FR_MAP_W - 1 && next_ty < FR_MAP_H - 1 &&
                   fr_is_walkable(fr_get_terrain(game, (uint8_t)next_tx, (uint8_t)next_ty))) {
                    actor->target_x = (uint8_t)next_tx;
                    actor->target_y = (uint8_t)next_ty;
                    diff_x = (int8_t)actor->target_x - (int8_t)actor->x;
                    diff_y = (int8_t)actor->target_y - (int8_t)actor->y;
                } else {
                    actor->memory = 0;
                    continue;
                }
            }
        } else {
            if(!afraid && fr_actor_notice_player(game, actor, manhattan)) {
                diff_x = (int8_t)actor->target_x - (int8_t)actor->x;
                diff_y = (int8_t)actor->target_y - (int8_t)actor->y;
            } else {
                if((actor->flags & FR_ACTOR_ROAMS) != 0) fr_actor_roam(game, actor);
                else fr_actor_idle_tremble(game, actor);
                continue;
            }
        }

        if(afraid) {
            diff_x = (int8_t)actor->x - (int8_t)game->player.x;
            diff_y = (int8_t)actor->y - (int8_t)game->player.y;
        }

        int8_t dx = 0;
        int8_t dy = 0;
        if((actor->effects & FR_FX_CONFUSED) != 0) {
            dx = (int8_t)((int)(fr_rand(game) % 3u) - 1);
            dy = dx == 0 ? (int8_t)((int)(fr_rand(game) % 3u) - 1) : 0;
        } else if(actor->type == FR_MON_BAT && actor->memory < 18 && (fr_rand(game) & 1u) == 0) {
            dx = (int8_t)((int)(fr_rand(game) % 3u) - 1);
        } else if(!fr_find_path_step_current(game, actor, actor->target_x, actor->target_y, &dx, &dy)) {
            if(fr_abs_i8(diff_x) > fr_abs_i8(diff_y)) dx = diff_x > 0 ? 1 : -1;
            else if(diff_y != 0) dy = diff_y > 0 ? 1 : -1;
            else if(diff_x != 0) dx = diff_x > 0 ? 1 : -1;
        }
        fr_try_move_actor_current(game, actor, dx, dy);
    }
}
