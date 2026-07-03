#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_identify_one_unknown(FrGame* game, uint8_t skip_index);
bool fr_identify_inventory_slot(FrGame* game, uint8_t target_index);
