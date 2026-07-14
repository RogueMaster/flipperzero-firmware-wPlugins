// Enhanced Pokemon Structures with pokeyellow integration
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pokemon.h"

// Enhanced Pokemon structure with more authentic data
typedef struct {
    char name[16];
    PokemonType type1;
    PokemonType type2; // For dual-type Pokemon
    uint8_t level;
    uint8_t pokedex_id;

    // Base stats (from pokeyellow)
    uint8_t base_hp;
    uint8_t base_attack;
    uint8_t base_defense;
    uint8_t base_speed;
    uint8_t base_special;

    // Current battle stats
    uint16_t max_hp;
    uint16_t current_hp;
    uint16_t attack;
    uint16_t defense;
    uint16_t speed;
    uint16_t special;

    // Status conditions
    uint8_t status; // Poison, burn, sleep, etc.
    uint8_t status_turns;

    // Moves (up to 4)
    Move moves[4];
    uint8_t move_pp[4]; // Current PP for each move

    // Experience and growth
    uint32_t experience;
    uint8_t growth_rate;

    // Individual Values (IVs) - for stat variation
    uint8_t iv_hp;
    uint8_t iv_attack;
    uint8_t iv_defense;
    uint8_t iv_speed;
    uint8_t iv_special;
} EnhancedPokemon;

// Status condition flags
#define STATUS_NONE     0x00
#define STATUS_SLEEP    0x01
#define STATUS_POISON   0x02
#define STATUS_BURN     0x04
#define STATUS_FREEZE   0x08
#define STATUS_PARALYZE 0x10

// Growth rates (from pokeyellow)
typedef enum {
    GROWTH_MEDIUM_FAST,
    GROWTH_ERRATIC,
    GROWTH_FLUCTUATING,
    GROWTH_MEDIUM_SLOW,
    GROWTH_FAST,
    GROWTH_SLOW
} GrowthRate;

// Enhanced move structure with more data
typedef struct {
    const char* name;
    uint8_t power;
    PokemonType type;
    uint8_t accuracy;
    uint8_t max_pp;
    uint8_t effect; // Special effects
    uint8_t priority; // Move priority
    const char* description;
} EnhancedMove;

// Move effects
#define EFFECT_NONE         0
#define EFFECT_BURN         1
#define EFFECT_FREEZE       2
#define EFFECT_PARALYZE     3
#define EFFECT_POISON       4
#define EFFECT_SLEEP        5
#define EFFECT_STAT_UP      6
#define EFFECT_STAT_DOWN    7
#define EFFECT_CRITICAL_HIT 8

// Function declarations for enhanced Pokemon system
EnhancedPokemon* enhanced_pokemon_create(uint8_t pokedex_id, uint8_t level);
void enhanced_pokemon_free(EnhancedPokemon* pokemon);
void enhanced_pokemon_calculate_stats(EnhancedPokemon* pokemon);
uint16_t enhanced_pokemon_calculate_damage(
    EnhancedPokemon* attacker,
    EnhancedPokemon* defender,
    uint8_t move_index);
void enhanced_pokemon_apply_status(EnhancedPokemon* pokemon, uint8_t status);
bool enhanced_pokemon_can_move(EnhancedPokemon* pokemon);
void enhanced_pokemon_level_up(EnhancedPokemon* pokemon);

// Data access functions
const PokemonSpecies* pokemon_get_yellow_species_list(void);
int pokemon_get_yellow_species_count(void);
const Move* pokemon_get_yellow_moves_list(void);
int pokemon_get_yellow_moves_count(void);

// Type effectiveness system
float get_type_effectiveness(
    PokemonType attack_type,
    PokemonType defend_type1,
    PokemonType defend_type2);

// Battle mechanics
typedef struct {
    EnhancedPokemon* player_pokemon;
    EnhancedPokemon* enemy_pokemon;
    uint8_t turn_count;
    bool battle_over;
    bool player_won;
    char battle_log[256];
} EnhancedBattle;

EnhancedBattle* enhanced_battle_create(EnhancedPokemon* player, EnhancedPokemon* enemy);
void enhanced_battle_free(EnhancedBattle* battle);
bool enhanced_battle_execute_turn(EnhancedBattle* battle, uint8_t player_move, uint8_t enemy_move);
const char* enhanced_battle_get_log(EnhancedBattle* battle);
