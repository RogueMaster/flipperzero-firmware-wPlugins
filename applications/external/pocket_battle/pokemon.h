// Pokemon Structures and definitions
#pragma once // Include guard - prevents double inclusion

#include <stdint.h> // For uint8_t, uint16_t types

// Enums are just named numbers
// This tells other files "a Pokemon type exists with these values"
typedef enum {
    TYPE_NORMAL,
    TYPE_FIRE,
    TYPE_WATER,
    TYPE_GRASS,
    TYPE_ELECTRIC,
    TYPE_FIGHTING,
    TYPE_POISON,
    TYPE_GROUND,
    TYPE_FLYING,
    TYPE_PSYCHIC,
    TYPE_BUG,
    TYPE_ROCK,
    TYPE_GHOST,
    TYPE_DRAGON,
    TYPE_DARK,
    TYPE_STEEL,
    TYPE_FAIRY,
    TYPE_ICE,
    TYPE_NULL, // Special type for uninitialized or invalid Pokemon
    TYPE_COUNT // Trick: This equals 18 (the count)
} PokemonType;

typedef struct {
    const char* name;
    uint8_t power;
    PokemonType type;
    uint8_t accuracy;
    uint8_t pp;
} Move;

// Structure definition
// This tells other files "a Pokemon structure exists with these fields"
typedef struct {
    char name[16]; // Array of 16 chars
    PokemonType type; // Our enum
    uint8_t level;
    uint8_t pokedexId;
    uint16_t max_hp;
    uint16_t current_hp;
    uint8_t attack;
    uint8_t defense;
    uint8_t speed;
    Move moves[4]; // Array of 4 moves
} Pokemon;

// Base Pokemon data (like from your CSV)
typedef struct {
    const char* name;
    PokemonType type;
    uint16_t base_hp;
    uint8_t base_attack;
    uint8_t base_defense;
    uint8_t base_speed;
} PokemonSpecies;

// Function declarations (prototypes)
//  These are function "promises" - implemented in pokemon.c
Pokemon* pokemon_create(const char* name, PokemonType type, uint8_t level);
Pokemon* pokemon_create_from_species(const PokemonSpecies* species, uint8_t level, int pokedexId);
void pokemon_free(Pokemon* pokemon);
uint16_t pokemon_calculate_damage(Pokemon* attacker, Pokemon* defender, uint8_t move_power);

// Get available Pokemon
const PokemonSpecies* pokemon_get_species_list(void);
int pokemon_get_species_count(void);
