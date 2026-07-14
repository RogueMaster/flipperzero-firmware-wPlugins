#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_award_xp(FrGame* game, uint8_t amount);
bool fr_apply_perk(FrGame* game, uint8_t choice);
const char* fr_perk_name(uint8_t class_id, uint8_t choice);
const char* fr_perk_desc(uint8_t class_id, uint8_t choice);
const char* fr_player_class_name(uint8_t class_id);
