#pragma once

#include "include/domain/game_limits.h"
#include <stdbool.h>
#include <stdint.h>

bool game_rules_validate_player_count(uint8_t n);

bool game_rules_validate_impostor_count(uint8_t player_count,
                                        uint8_t impostor_count);

uint8_t game_rules_max_impostors(uint8_t player_count);

uint8_t game_rules_suggested_impostors(uint8_t player_count);
