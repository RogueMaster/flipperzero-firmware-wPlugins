#pragma once

#include "p1x_network_defender.h"

// Game logic functions
void update_game(GameState* state);
void update_upcoming_packets(GameState* state);
uint32_t calculate_spawn_interval(GameState* state);
