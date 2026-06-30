#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TeamRed,
    TeamBlue
} LaserTagTeam;

typedef enum {
    LaserTagStateSplashScreen,
    LaserTagStateTeamSelect,
    LaserTagStateGame,
    LaserTagStateGameOver,
} LaserTagState;

typedef struct GameState GameState;

GameState* game_state_alloc();
void game_state_reset(GameState* state);

void game_state_set_team(GameState* state, LaserTagTeam team);
LaserTagTeam game_state_get_team(GameState* state);

void game_state_decrease_health(GameState* state, uint8_t amount);
void game_state_increase_health(GameState* state, uint8_t amount);
uint8_t game_state_get_health(GameState* state);

void game_state_decrease_ammo(GameState* state, uint16_t amount);
void game_state_increase_ammo(GameState* state, uint16_t amount);
uint16_t game_state_get_ammo(GameState* state);

void game_state_update_time(GameState* state, uint32_t delta_time);
uint32_t game_state_get_time(GameState* state);

bool game_state_is_game_over(GameState* state);
void game_state_set_game_over(GameState* state, bool game_over);

#define INITIAL_HEALTH 100
#define INITIAL_AMMO   100
#define MAX_HEALTH     100
