#pragma once

#include "game_logic.h"

#include <stdint.h>

void fr_apply_potion_to_player(FrGame* game, uint8_t potion);
void fr_apply_potion_to_actor(FrGame* game, FrActor* actor, uint8_t potion);
void fr_throw_potion_at(FrGame* game, uint8_t potion, uint8_t tx, uint8_t ty);
