#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_set_game_over(FrGame* game, uint8_t cause, const char* fmt, ...);
void fr_kill_actor(FrGame* game, FrActor* actor);
void fr_damage_actor_kind(FrGame* game, FrActor* actor, uint8_t damage, const char* verb, uint8_t kind);
void fr_damage_actor(FrGame* game, FrActor* actor, uint8_t damage, const char* verb);
bool fr_force_push_actor(FrGame* game, FrActor* actor, int8_t dx, int8_t dy);
void fr_actor_hit_player_effects(FrGame* game, FrActor* actor);
void fr_cube_tick(FrGame* game);
void fr_hit_cube_from_inside(FrGame* game, uint8_t damage);
