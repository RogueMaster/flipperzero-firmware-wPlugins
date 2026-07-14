#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

uint8_t fr_wand_max_charges(uint8_t wand);
uint8_t fr_trinket_break_turns(uint8_t trinket);
FrInvSlot* fr_equipped_trinket_slot(FrGame* game);
bool fr_has_equipped_trinket(const FrGame* game, uint8_t trinket);
