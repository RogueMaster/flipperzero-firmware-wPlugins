#ifndef POKEMON_ATTRIBUTE_H
#define POKEMON_ATTRIBUTE_H

#pragma once

#include <src/include/pokemon_data.h>

typedef enum {
    GENDER_MALE,
    GENDER_FEMALE,
} Gender;

typedef enum {
    GENDER_F0 = 0x00,
    GENDER_F12_5 = 0x1F,
    GENDER_F25 = 0x3F,
    GENDER_F50 = 0x7F,
    GENDER_F75 = 0xBF,
    GENDER_F100 = 0xFE,
    GENDER_UNKNOWN = 0xFF,
} GenderRatio;

/* The gender ratio is a bit value, and if the ATK_IV is less than or equal to
 * the gender ratio, the gender is female.
 *
 * A ratio of 0xff means gender is unknown.
 * A ratio of 0x00 is annoyingly special. It either means that pokemon can be
 *   male only -OR- there is a very small chance the pokemon is female. The
 *   male only pokemon need to be specifically checked for.
 */

const char* pokemon_gender_is_static(PokemonData* pdata, uint8_t ratio);

/* This will return a pointer to a string of the pokemon's current gender */
const char* pokemon_gender_get(PokemonData* pdata);

void pokemon_gender_set(PokemonData* pdata, Gender gender);

const char* pokerus_get_status_str(PokemonData* pdata);

void pokerus_set_strain(PokemonData* pdata, uint8_t strain);

void pokerus_set_days(PokemonData* pdata, uint8_t days);

bool pokemon_is_shiny(PokemonData* pdata);

void pokemon_set_shiny(PokemonData* pdata, bool shiny);

/* Returns ascii char, or 0 if unown is not the current pokemon */
char unown_form_get(PokemonData* pdata);

void unown_form_set(PokemonData* pdata, char letter);

#endif // POKEMON_ATTRIBUTE_H
