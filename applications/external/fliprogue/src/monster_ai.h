#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_try_move_actor_current(FrGame* game, FrActor* actor, int8_t dx, int8_t dy);
void fr_actor_turns(FrGame* game);
