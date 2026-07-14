#pragma once

#include "p1x_network_defender.h"

// Game state functions
void game_init(GameState* state);
void reset_game_state(GameState* state);
bool load_high_score(GameState* state);
bool save_high_score(GameState* state);
