#include "hazards.h"

#include "combat.h"
#include "equipment.h"
#include "game_core.h"
#include "map_state.h"
#include "placement.h"
#include "status_effects.h"
#include "terrain_effects.h"

bool fr_player_resists_fire(const FrGame* game) {
    return game->player.fire_ward_timer > 0 || fr_has_equipped_trinket(game, FR_TRINKET_ASH);
}

uint8_t fr_player_fire_damage(const FrGame* game, uint8_t base_damage) {
    if(base_damage == 0) return 0;
    return fr_has_equipped_trinket(game, FR_TRINKET_ASH) ? 1 : base_damage;
}

uint8_t fr_fire_damage_roll(FrGame* game) {
    return (uint8_t)(1 + fr_rand_u8(game, 3));
}

void fr_apply_fire_to_player(FrGame* game, uint8_t damage, uint8_t burning_turns) {
    uint8_t actual = fr_player_fire_damage(game, damage);
    if(actual > 0 && game->player.hp > 0) {
        if(game->player.hp > actual) game->player.hp = (uint8_t)(game->player.hp - actual);
        else game->player.hp = 0;
    }
    if(!fr_player_resists_fire(game) && burning_turns > 0) {
        fr_apply_effect_to_player(&game->player, FR_FX_BURNING, FR_FX_BURNING_INDEX, burning_turns);
    }
}

void fr_fire_burst(FrGame* game, uint8_t cx, uint8_t cy) {
    bool hiss = false;
    for(int8_t oy = -1; oy <= 1; oy++) {
        for(int8_t ox = -1; ox <= 1; ox++) {
            int16_t x_i = (int16_t)cx + ox;
            int16_t y_i = (int16_t)cy + oy;
            if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
            uint8_t x = (uint8_t)x_i;
            uint8_t y = (uint8_t)y_i;
            uint8_t terrain = fr_get_terrain(game, x, y);
            if(terrain == FR_TERR_PUDDLE || terrain == FR_TERR_WATER || terrain == FR_TERR_SAND ||
               terrain == FR_TERR_ICE || terrain == FR_TERR_SHRINE || fr_blocking_item_at(game, x, y)) {
                hiss = true;
                continue;
            }
            if(terrain == FR_TERR_GRASS) fr_set_terrain(game, x, y, FR_TERR_GRASS_TRAMPLED);
            if(game->player.x == x && game->player.y == y) {
                uint8_t damage = fr_fire_damage_roll(game);
                fr_apply_fire_to_player(game, damage, 5);
                if(game->player.cube_hp > 0) fr_hit_cube_from_inside(game, 1);
            }
            FrActor* actor = fr_actor_at(game, x, y);
            if(actor) {
                fr_damage_actor_kind(game, actor, fr_fire_damage_roll(game), "Fire sears", FR_DAMAGE_BURST);
                if(actor->active) fr_apply_effect_to_actor(actor, FR_FX_BURNING, FR_FX_BURNING_INDEX, 5);
            }
        }
    }
    fr_refresh_or_expand_fire_field(game, game->floor, cx, cy, 1);
    fr_log(game, hiss ? "Fire hisses." : "Fire blooms.");
}

void fr_trigger_trap(FrGame* game, FrTrap* trap) {
    if(!trap || !trap->active) return;
    trap->active = false;
    if(trap->type == FR_TRAP_ARROW) {
        fr_event_projectile(game, trap->source_x, trap->source_y, game->player.x, game->player.y, '-');
        uint8_t damage = 2;
        if(game->player.hp > damage) {
            game->player.hp = (uint8_t)(game->player.hp - damage);
            fr_log(game, "Arrow trap.");
        } else {
            game->player.hp = 0;
            fr_set_game_over(game, FR_DEATH_KILLED, "Killed by arrow trap.");
        }
    } else if(trap->type == FR_TRAP_FIRE) {
        fr_event_projectile(game, trap->source_x, trap->source_y, game->player.x, game->player.y, '*');
        fr_log(game, "Oil jar bursts.");
        fr_fire_burst(game, trap->x, trap->y);
    } else if(trap->type == FR_TRAP_POISON) {
        fr_apply_effect_to_player(&game->player, FR_FX_POISONED, FR_FX_POISONED_INDEX, 12);
        fr_log(game, "Poison trap.");
    } else {
        fr_apply_effect_to_player(&game->player, FR_FX_STUNNED, FR_FX_STUNNED_INDEX, 4);
        game->last_event = FR_EVENT_SNARE;
        fr_log(game, "Snare locks.");
    }
}
