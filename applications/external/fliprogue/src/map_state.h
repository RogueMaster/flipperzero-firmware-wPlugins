#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

uint8_t fr_tile_core(const FrGame* game, uint8_t x, uint8_t y);
void fr_record_tile_delta(FrGame* game, uint8_t x, uint8_t y);
void fr_mark_hidden_door(FrGame* game, uint8_t x, uint8_t y);
bool fr_reveal_hidden_door_at(FrGame* game, uint8_t x, uint8_t y);
