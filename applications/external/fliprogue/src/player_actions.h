#pragma once

#include "game_logic.h"

#include <stdint.h>

FrActionResult fr_try_direction(FrGame* game, int8_t dx, int8_t dy);
FrActionResult fr_move_player(FrGame* game, int8_t dx, int8_t dy);
FrActionResult fr_rest(FrGame* game);
FrActionResult fr_eat_food(FrGame* game);
