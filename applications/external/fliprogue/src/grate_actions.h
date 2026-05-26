#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_place_grate(FrGame* game, uint8_t x, uint8_t y);
bool fr_place_key(FrGame* game, uint8_t x, uint8_t y);
bool fr_place_button(FrGame* game, uint8_t x, uint8_t y);
bool fr_open_grates_on_floor(FrGame* game);
