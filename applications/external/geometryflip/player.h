#pragma once
#include "constants.h"

// Function prototypes for player management
void player_init(PlayerState* player);
void player_update(PlayerState* player, uint32_t dt_ms, bool input_pressed, GameState state);
void player_handle_input(PlayerState* player, bool input_pressed);
void player_reset(PlayerState* player);
