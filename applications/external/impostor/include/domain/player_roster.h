#pragma once

#include "include/domain/game_limits.h"
#include <stdint.h>

void player_roster_remove_at(char names[][IMPOSTOR_MAX_NAME_LEN],
                             uint8_t *name_count, uint8_t index);
