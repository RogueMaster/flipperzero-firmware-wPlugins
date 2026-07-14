#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_item_at_xy(const FrGame* game, uint8_t x, uint8_t y);
bool fr_drop_inventory_item(FrGame* game, uint8_t index);
void fr_pickup_at_player(FrGame* game);
