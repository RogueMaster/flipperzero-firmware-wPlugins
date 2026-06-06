#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_place_shrine(FrGame* game, uint8_t x, uint8_t y);
FrActionResult fr_use_shrine_at(FrGame* game, uint8_t x, uint8_t y);
FrActionResult fr_use_shrine(FrGame* game);
