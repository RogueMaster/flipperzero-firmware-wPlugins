// Pokemon Sprites for Flipper Zero
#pragma once

#include <gui/icon.h>

#include "pokemon_pikachu.xbm"
#include "pokemon_charmander.xbm"
#include "pokemon_squirtle.xbm"
#include "pokemon_bulbasaur.xbm"
#include "pokemon_charizard.xbm"
#include "pokemon_blastoise.xbm"
#include "pokemon_venusaur.xbm"
#include "pokemon_mew.xbm"
#include "pokemon_mewtwo.xbm"
#include "pokemon_gengar.xbm"
#include "pokemon_alakazam.xbm"
#include "pokemon_machamp.xbm"

// Sprite data structure
typedef struct {
    const char* name;
    const unsigned char* data;
    int width;
    int height;
} PokemonSprite;

// Sprite array
static const PokemonSprite pokemon_sprites[] = {
    {"pikachu", pikachu_bits, pikachu_width, pikachu_height},
    {"charmander", charmander_bits, charmander_width, charmander_height},
    {"squirtle", squirtle_bits, squirtle_width, squirtle_height},
    {"bulbasaur", bulbasaur_bits, bulbasaur_width, bulbasaur_height},
    {"charizard", charizard_bits, charizard_width, charizard_height},
    {"blastoise", blastoise_bits, blastoise_width, blastoise_height},
    {"venusaur", venusaur_bits, venusaur_width, venusaur_height},
    {"mew", mew_bits, mew_width, mew_height},
    {"mewtwo", mewtwo_bits, mewtwo_width, mewtwo_height},
    {"gengar", gengar_bits, gengar_width, gengar_height},
    {"alakazam", alakazam_bits, alakazam_width, alakazam_height},
    {"machamp", machamp_bits, machamp_width, machamp_height},
};

#define POKEMON_SPRITE_COUNT 12

// Function to get sprite by name
const PokemonSprite* get_pokemon_sprite(const char* name);
