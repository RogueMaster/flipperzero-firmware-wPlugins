#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_place_chest(FrGame* game, uint8_t x, uint8_t y, bool mimic);
FrItem* fr_chest_at(FrGame* game, uint8_t x, uint8_t y);
FrItem* fr_adjacent_chest_for_bump(FrGame* game, uint8_t x, uint8_t y);
FrActionResult fr_bump_chest(FrGame* game, FrItem* chest);
uint8_t fr_chest_choice_count(const FrGame* game, const FrItem* chest);
FrInvSlot fr_chest_choice_slot(const FrGame* game, const FrItem* chest, uint8_t choice);
FrActionResult fr_open_chest_choice(FrGame* game, FrItem* chest, uint8_t choice);
FrActionResult fr_cancel_chest(FrGame* game, FrItem* chest);
