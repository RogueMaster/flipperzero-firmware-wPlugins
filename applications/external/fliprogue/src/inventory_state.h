#pragma once

#include "game_logic.h"

#include <stdint.h>

void fr_remove_inventory(FrGame* game, uint8_t index);
void fr_consume_inventory(FrGame* game, uint8_t index);
uint8_t fr_inventory_used_count(const FrGame* game);
