// Pokemon Integration Layer - Connects pokeyellow data with showdown game
#include "pokemon_integration.h"
#include "pokemon_enhanced.h"
#include "enhanced_battle_screen.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

// Global game state
static PokemonGameState game_state = {0};

void pokemon_integration_init(void) {
    // Initialize random seed
    srand(furi_get_tick());

    // Initialize game state
    game_state.player_team_count = 0;
    game_state.current_battle = NULL;
    game_state.game_mode = GAME_MODE_MENU;

    // Create a default starter team
    game_state.player_team[0] = enhanced_pokemon_create(25, 5); // Pikachu
    game_state.player_team[1] = enhanced_pokemon_create(1, 5); // Bulbasaur
    game_state.player_team[2] = enhanced_pokemon_create(4, 5); // Charmander
    game_state.player_team_count = 3;

    // Set active Pokemon
    game_state.active_player_pokemon = 0;
}

void pokemon_integration_cleanup(void) {
    // Free player team
    for(int i = 0; i < game_state.player_team_count; i++) {
        if(game_state.player_team[i]) {
            enhanced_pokemon_free(game_state.player_team[i]);
            game_state.player_team[i] = NULL;
        }
    }

    // Free current battle
    if(game_state.current_battle) {
        enhanced_battle_free(game_state.current_battle);
        game_state.current_battle = NULL;
    }
}

EnhancedPokemon* pokemon_integration_create_wild_pokemon(uint8_t min_level, uint8_t max_level) {
    // Create a random wild Pokemon
    int species_count = pokemon_get_yellow_species_count();
    uint8_t random_species = rand() % species_count;
    uint8_t random_level = min_level + (rand() % (max_level - min_level + 1));

    return enhanced_pokemon_create(random_species, random_level);
}

void pokemon_integration_start_wild_battle(void) {
    if(game_state.player_team_count == 0) return;

    // Create wild Pokemon
    EnhancedPokemon* wild_pokemon = pokemon_integration_create_wild_pokemon(3, 7);

    // Get player's active Pokemon
    EnhancedPokemon* player_pokemon = game_state.player_team[game_state.active_player_pokemon];

    // Start battle
    game_state.game_mode = GAME_MODE_BATTLE;
    enhanced_battle_screen_show(player_pokemon, wild_pokemon);

    // Cleanup
    enhanced_pokemon_free(wild_pokemon);
    game_state.game_mode = GAME_MODE_MENU;
}

void pokemon_integration_start_trainer_battle(uint8_t trainer_id) {
    if(game_state.player_team_count == 0) return;

    // Create trainer Pokemon based on trainer ID
    EnhancedPokemon* trainer_pokemon = NULL;

    switch(trainer_id) {
    case 0: // Youngster
        trainer_pokemon = enhanced_pokemon_create(19, 6); // Rattata
        break;
    case 1: // Bug Catcher
        trainer_pokemon = enhanced_pokemon_create(13, 7); // Weedle
        break;
    case 2: // Lass
        trainer_pokemon = enhanced_pokemon_create(35, 8); // Clefairy
        break;
    default:
        trainer_pokemon = enhanced_pokemon_create(25, 10); // Pikachu
        break;
    }

    // Get player's active Pokemon
    EnhancedPokemon* player_pokemon = game_state.player_team[game_state.active_player_pokemon];

    // Start battle
    game_state.game_mode = GAME_MODE_BATTLE;
    enhanced_battle_screen_show(player_pokemon, trainer_pokemon);

    // Cleanup
    enhanced_pokemon_free(trainer_pokemon);
    game_state.game_mode = GAME_MODE_MENU;
}

bool pokemon_integration_add_pokemon_to_team(EnhancedPokemon* pokemon) {
    if(game_state.player_team_count >= MAX_TEAM_SIZE) {
        return false; // Team is full
    }

    game_state.player_team[game_state.player_team_count] = pokemon;
    game_state.player_team_count++;

    return true;
}

EnhancedPokemon* pokemon_integration_get_active_pokemon(void) {
    if(game_state.player_team_count == 0) return NULL;
    return game_state.player_team[game_state.active_player_pokemon];
}

void pokemon_integration_switch_active_pokemon(uint8_t index) {
    if(index < game_state.player_team_count) {
        game_state.active_player_pokemon = index;
    }
}

PokemonGameState* pokemon_integration_get_game_state(void) {
    return &game_state;
}

// Utility functions for data access
const char* pokemon_integration_get_type_name(PokemonType type) {
    switch(type) {
    case TYPE_NORMAL:
        return "Normal";
    case TYPE_FIRE:
        return "Fire";
    case TYPE_WATER:
        return "Water";
    case TYPE_GRASS:
        return "Grass";
    case TYPE_ELECTRIC:
        return "Electric";
    case TYPE_FIGHTING:
        return "Fighting";
    case TYPE_POISON:
        return "Poison";
    case TYPE_GROUND:
        return "Ground";
    case TYPE_FLYING:
        return "Flying";
    case TYPE_PSYCHIC:
        return "Psychic";
    case TYPE_BUG:
        return "Bug";
    case TYPE_ROCK:
        return "Rock";
    case TYPE_GHOST:
        return "Ghost";
    case TYPE_DRAGON:
        return "Dragon";
    case TYPE_DARK:
        return "Dark";
    case TYPE_STEEL:
        return "Steel";
    case TYPE_FAIRY:
        return "Fairy";
    case TYPE_ICE:
        return "Ice";
    default:
        return "Unknown";
    }
}

void pokemon_integration_heal_team(void) {
    for(int i = 0; i < game_state.player_team_count; i++) {
        if(game_state.player_team[i]) {
            game_state.player_team[i]->current_hp = game_state.player_team[i]->max_hp;
            game_state.player_team[i]->status = STATUS_NONE;

            // Restore PP
            for(int j = 0; j < 4; j++) {
                game_state.player_team[i]->move_pp[j] = game_state.player_team[i]->moves[j].pp;
            }
        }
    }
}

// Save/Load functionality (simplified)
bool pokemon_integration_save_game(const char* filename) {
    // In a real implementation, you'd save to Flipper Zero storage
    // For now, just return success
    UNUSED(filename);
    return true;
}

bool pokemon_integration_load_game(const char* filename) {
    // In a real implementation, you'd load from Flipper Zero storage
    // For now, just return success
    UNUSED(filename);
    return true;
}
