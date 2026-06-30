#ifndef POKEMON_TABLE_H
#define POKEMON_TABLE_H

#include <stdint.h>

#include <src/include/stats.h>

typedef enum {
    GROWTH_MEDIUM_FAST = 0,
    GROWTH_MEDIUM_SLOW = 3,
    GROWTH_FAST = 4,
    GROWTH_SLOW = 5,
} Growth;

typedef struct pokemon_data_table PokemonTable;

int table_pokemon_pos_get(const PokemonTable* table, uint8_t index);
uint8_t
    table_stat_base_get(const PokemonTable* table, uint8_t num, DataStat stat, DataStatSub which);
const char* table_stat_name_get(const PokemonTable* table, int num);
const PokemonTable* table_pointer_get();

#endif // POKEMON_TABLE_H
