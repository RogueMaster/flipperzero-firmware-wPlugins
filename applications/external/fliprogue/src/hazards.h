#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_player_resists_fire(const FrGame* game);
uint8_t fr_player_fire_damage(const FrGame* game, uint8_t base_damage);
uint8_t fr_fire_damage_roll(FrGame* game);
void fr_apply_fire_to_player(FrGame* game, uint8_t damage, uint8_t burning_turns);
void fr_fire_burst(FrGame* game, uint8_t cx, uint8_t cy);
void fr_trigger_trap(FrGame* game, FrTrap* trap);
