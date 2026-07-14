#pragma once

#include "game_logic.h"

#include <stdint.h>

void fr_apply_effect_to_actor(FrActor* actor, uint8_t effect, uint8_t index, uint8_t turns);
void fr_apply_effect_to_player(FrPlayer* player, uint8_t effect, uint8_t index, uint8_t turns);
void fr_clear_effect_from_player(FrPlayer* player, uint8_t effect, uint8_t index);
void fr_reveal_actor(FrActor* actor);
void fr_tick_effects(FrGame* game);
void fr_tick_hunger(FrGame* game);
