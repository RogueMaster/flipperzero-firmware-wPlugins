#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_reveal_secrets(FrGame* game, uint8_t cx, uint8_t cy, uint8_t radius);
