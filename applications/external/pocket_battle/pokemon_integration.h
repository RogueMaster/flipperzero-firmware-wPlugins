// Pokemon Integration Header
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pokemon_enhanced.h"

#define MAX_TEAM_SIZE 6

// Game modes
typedef enum {
    GAME_MODE_MENU,
    GAME_MODE_BATTLE,
    GAME_MODE_TEAM,
    GAME_MODE_BAG,
    GAME_MODE_POKEDEX
} GameMode;

// Game state structure
typedef struct {
    EnhancedPokemon* player_team[MAX_TEAM_SIZE];
    uint8_t player_team_count;
    uint8_t active_player_pokemon;

    EnhancedBattle* current_battle;
    GameMode game_mode;

    // Game progress
    uint8_t badges_earned;
    uint32_t money;
    uint16_t pokedex_seen;
    uint16_t pokedex_caught;
} PokemonGameState;

// Core integration functions
void pokemon_integration_init(void);
void pokemon_integration_cleanup(void);

// Battle functions
EnhancedPokemon* pokemon_integration_create_wild_pokemon(uint8_t min_level, uint8_t max_level);
void pokemon_integration_start_wild_battle(void);
void pokemon_integration_start_trainer_battle(uint8_t trainer_id);

// Team management
bool pokemon_integration_add_pokemon_to_team(EnhancedPokemon* pokemon);
EnhancedPokemon* pokemon_integration_get_active_pokemon(void);
void pokemon_integration_switch_active_pokemon(uint8_t index);
void pokemon_integration_heal_team(void);

// Game state access
PokemonGameState* pokemon_integration_get_game_state(void);

// Utility functions
const char* pokemon_integration_get_type_name(PokemonType type);

// Save/Load
bool pokemon_integration_save_game(const char* filename);
bool pokemon_integration_load_game(const char* filename);
