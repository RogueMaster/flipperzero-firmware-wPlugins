#pragma once

#include "game_logic.h"

#include <stdint.h>

FrActionResult fr_use_inventory(FrGame* game, uint8_t index, uint8_t use_action, uint8_t tx, uint8_t ty);
