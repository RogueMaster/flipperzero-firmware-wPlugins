#include "game_internal.h"
#include "chest_actions.h"
#include "grate_actions.h"
#include "ice_effects.h"
#include "shrine_actions.h"

static bool fr_extinguish_player_if_burning(FrGame* game) {
    if((game->player.effects & FR_FX_BURNING) == 0) return false;
    fr_clear_effect_from_player(&game->player, FR_FX_BURNING, FR_FX_BURNING_INDEX);
    fr_log(game, "Doused.");
    return true;
}

FrActionResult fr_try_direction(FrGame* game, int8_t dx, int8_t dy) {
    if((dx != 0 && dy != 0) || (dx == 0 && dy == 0)) return fr_rest(game);
    int16_t adj_x = (int16_t)game->player.x + dx;
    int16_t adj_y = (int16_t)game->player.y + dy;
    if(adj_x >= 0 && adj_y >= 0 && adj_x < FR_MAP_W && adj_y < FR_MAP_H &&
       fr_actor_at(game, (uint8_t)adj_x, (uint8_t)adj_y) != NULL) {
        return fr_move_player(game, dx, dy);
    }

    if(game->player.class_id == FR_CLASS_RANGER || game->player.class_id == FR_CLASS_MAGE) {
        FrActor* actor = fr_line_actor(game, dx, dy);
        if(actor) {
            game->log[0] = '\0';
            if(game->player.class_id == FR_CLASS_RANGER && !fr_player_can_ranged(game)) {
                return fr_move_player(game, dx, dy);
            }
            if(game->player.class_id == FR_CLASS_MAGE && !fr_player_can_ranged(game)) {
                return fr_move_player(game, dx, dy);
            }
            if(game->player.class_id == FR_CLASS_RANGER) game->player.arrows--;
            if(game->player.class_id == FR_CLASS_MAGE) game->player.charges--;
            uint8_t distance = (uint8_t)(
                fr_abs_i8((int8_t)actor->x - (int8_t)game->player.x) +
                fr_abs_i8((int8_t)actor->y - (int8_t)game->player.y));
            uint8_t damage = fr_player_ranged_damage(game);
            if(game->player.class_id == FR_CLASS_RANGER && (game->player.perks & FR_PERK_4) != 0 &&
               distance >= 4) {
                damage++;
            }
            if(game->player.class_id == FR_CLASS_MAGE && (game->player.perks & FR_PERK_1) != 0 &&
               fr_rand_u8(game, 100) < 10) {
                damage = (uint8_t)(damage * 2);
                fr_log(game, "Overcharge.");
            }
            fr_damage_actor_kind(game, actor, damage, "Shot", FR_DAMAGE_PROJECTILE);
            if(actor->active && game->player.class_id == FR_CLASS_RANGER) {
                if((game->player.perks & FR_PERK_1) != 0 && fr_rand_u8(game, 100) < 10) {
                    fr_force_push_actor(game, actor, dx, dy);
                }
                if((game->player.perks & FR_PERK_2) != 0 && fr_rand_u8(game, 100) < 10) {
                    fr_apply_effect_to_actor(actor, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 6);
                }
            } else if(actor->active && game->player.class_id == FR_CLASS_MAGE) {
                if((game->player.perks & FR_PERK_2) != 0 && fr_rand_u8(game, 100) < 10) {
                    fr_apply_effect_to_actor(actor, FR_FX_BURNING, FR_FX_BURNING_INDEX, 5);
                }
                if((game->player.perks & FR_PERK_3) != 0 && fr_rand_u8(game, 100) < 10) {
                    fr_apply_effect_to_actor(actor, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 6);
                }
            }
            fr_finish_turn(game);
            return (FrActionResult){FR_ACTION_RANGED};
        }
    }
    return fr_move_player(game, dx, dy);
}

FrActionResult fr_move_player(FrGame* game, int8_t dx, int8_t dy) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(game->mode != FR_MODE_PLAYING) return (FrActionResult){FR_ACTION_BLOCKED};
    if((dx != 0 && dy != 0) || (dx == 0 && dy == 0)) return fr_rest(game);

    if((game->player.effects & FR_FX_STUNNED) != 0) {
        fr_log(game, "Dazed.");
        fr_finish_turn(game);
        return (FrActionResult){FR_ACTION_REST};
    }

    if(game->player.cube_hp > 0) {
        fr_hit_cube_from_inside(game, fr_player_damage(game));
        fr_finish_turn(game);
        return (FrActionResult){FR_ACTION_ATTACK};
    }

    if((game->player.effects & FR_FX_AFRAID) != 0 && fr_fear_direction(game, &dx, &dy)) {
        fr_log(game, "Panic.");
    } else if((game->player.effects & FR_FX_CONFUSED) != 0) {
        uint8_t roll = fr_rand_u8(game, 4);
        dx = roll == 0 ? 1 : (roll == 1 ? -1 : 0);
        dy = roll == 2 ? 1 : (roll == 3 ? -1 : 0);
    }

    int16_t nx_i = (int16_t)game->player.x + dx;
    int16_t ny_i = (int16_t)game->player.y + dy;
    if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) {
        fr_log(game, "Edge of tiny world.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }

    uint8_t nx = (uint8_t)nx_i;
    uint8_t ny = (uint8_t)ny_i;
    FrItem* chest = fr_chest_at(game, nx, ny);
    if(chest) {
        return fr_bump_chest(game, chest);
    }
    FrActor* actor = fr_actor_at(game, nx, ny);
    if(actor) {
        if((actor->flags & FR_ACTOR_HIDDEN) != 0) {
            fr_reveal_actor(actor);
            fr_log(game, "Ambush.");
        }
        uint8_t damage = fr_player_damage(game);
        bool shield_bash = false;
        if(game->player.class_id == FR_CLASS_WARRIOR && (game->player.perks & FR_PERK_2) != 0 &&
           fr_rand_u8(game, 100) < 10) {
            damage = (uint8_t)(damage * 2);
            shield_bash = true;
        }
        fr_damage_actor(game, actor, damage, "You hit");
        if(actor->active && shield_bash) fr_force_push_actor(game, actor, dx, dy);
        if(!actor->active && game->player.class_id == FR_CLASS_WARRIOR && (game->player.perks & FR_PERK_4) != 0) {
            for(int8_t oy = -1; oy <= 1; oy++) {
                for(int8_t ox = -1; ox <= 1; ox++) {
                    if(ox == 0 && oy == 0) continue;
                    int16_t ax = (int16_t)nx + ox;
                    int16_t ay = (int16_t)ny + oy;
                    if(ax < 0 || ay < 0 || ax >= FR_MAP_W || ay >= FR_MAP_H) continue;
                    FrActor* near = fr_actor_at(game, (uint8_t)ax, (uint8_t)ay);
                    if(near) fr_damage_actor_kind(game, near, 1, "Cleave cuts", FR_DAMAGE_BURST);
                }
            }
        }
        fr_finish_turn(game);
        return (FrActionResult){FR_ACTION_ATTACK};
    }

    uint8_t terrain = fr_get_terrain(game, nx, ny);
    if(terrain == FR_TERR_DOOR_CLOSED) {
        fr_set_terrain(game, nx, ny, FR_TERR_DOOR_OPEN);
        terrain = FR_TERR_DOOR_OPEN;
        fr_log(game, "Door creaks.");
    }
    if(!fr_is_walkable(terrain)) {
        if(terrain == FR_TERR_SHRINE) {
            return fr_use_shrine_at(game, nx, ny);
        }
        if(fr_reveal_hidden_door_at(game, nx, ny)) {
            fr_log(game, "Hidden door.");
            fr_event_secret(game, nx, ny);
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        fr_log(game, terrain == FR_TERR_WALL ? "A rough stone wall." : "Blocked.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }

    game->player.x = nx;
    game->player.y = ny;
    if(terrain == FR_TERR_GRASS) {
        fr_set_terrain(game, nx, ny, FR_TERR_GRASS_TRAMPLED);
        game->last_event = FR_EVENT_NONE;
        uint8_t dew_roll = fr_has_equipped_trinket(game, FR_TRINKET_DEW) ? 5 : 10;
        if(fr_rand_u8(game, dew_roll) == 0 && game->player.hp < game->player.max_hp) {
            game->player.hp++;
            game->last_event = FR_EVENT_DEW;
            game->event_x = nx;
            game->event_y = ny;
            fr_log(game, "Dew +1 HP.");
        }
        fr_log(game, "Grass sighs.");
    } else if(terrain == FR_TERR_PUDDLE) {
        fr_extinguish_player_if_burning(game);
        fr_log(game, "Splash.");
    } else if(terrain == FR_TERR_WATER) {
        fr_extinguish_player_if_burning(game);
        fr_log(game, "Deep water.");
    } else if(terrain == FR_TERR_ICE) {
        fr_log(game, "Ice.");
    } else if(terrain == FR_TERR_SAND) {
        fr_log(game, "Sand whispers.");
    } else if(terrain == FR_TERR_STAIRS_DOWN) {
        fr_log(game, "Stairs down. Press OK.");
    } else if(terrain == FR_TERR_STAIRS_UP) {
        fr_log(game, "Stairs up. Press OK.");
    } else if(terrain == FR_TERR_SHRINE) {
        fr_log(game, "Old god. Press OK.");
    } else if(terrain == FR_TERR_BUTTON) {
        fr_log(game, "Button clicks.");
        fr_open_grates_on_floor(game);
    } else if(game->log[0] == '\0') {
        fr_log(game, "Step.");
    }

    if(terrain == FR_TERR_ICE && fr_try_ice_slide_player(game, dx, dy)) {
        nx = game->player.x;
        ny = game->player.y;
        terrain = fr_get_terrain(game, nx, ny);
    }

    FrTrap* trap = fr_trap_at(game, nx, ny);
    if(trap && !fr_player_detects_trap(game, nx, ny)) fr_trigger_trap(game, trap);
    fr_pickup_at_player(game);
    fr_search_nearby(game);
    fr_finish_world_turns(game, terrain == FR_TERR_WATER ? 2 : 1);
    return (FrActionResult){FR_ACTION_MOVE};
}

FrActionResult fr_rest(FrGame* game) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(game->mode != FR_MODE_PLAYING) return (FrActionResult){FR_ACTION_BLOCKED};
    if(fr_get_terrain(game, game->player.x, game->player.y) == FR_TERR_SHRINE) return fr_use_shrine(game);
    if(fr_get_terrain(game, game->player.x, game->player.y) == FR_TERR_BUTTON) {
        fr_log(game, "Button clicks.");
        fr_open_grates_on_floor(game);
        fr_finish_turn(game);
        return (FrActionResult){FR_ACTION_USE};
    }
    if(fr_hunger_state(game) == FR_HUNGER_STARVING) {
        fr_log(game, "Too starved to wait.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    if((game->turn % 3u) == 0 && game->player.hp < game->player.max_hp && fr_hunger_state(game) != FR_HUNGER_STARVING) {
        game->player.hp++;
        fr_log(game, "Rest. +1 HP.");
    } else {
        fr_log(game, "You wait.");
    }
    if(game->player.class_id == FR_CLASS_MAGE && (game->player.perks & FR_PERK_4) != 0 &&
       game->player.charges < 9 && (game->turn % 4u) == 0) {
        game->player.charges++;
        fr_log(game, "Focus +charge.");
    }
    fr_search_nearby(game);
    fr_finish_turn(game);
    return (FrActionResult){FR_ACTION_REST};
}

FrActionResult fr_eat_food(FrGame* game) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(game->player.food == 0) {
        fr_log(game, "No food.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    game->player.food--;
    uint16_t hunger = (uint16_t)game->player.hunger + 180u;
    game->player.hunger = hunger > 255u ? 255u : (uint8_t)hunger;
    uint8_t hp = (uint8_t)(game->player.hp + 3);
    game->player.hp = hp > game->player.max_hp ? game->player.max_hp : hp;
    fr_log(game, "You eat.");
    return (FrActionResult){FR_ACTION_USE};
}
