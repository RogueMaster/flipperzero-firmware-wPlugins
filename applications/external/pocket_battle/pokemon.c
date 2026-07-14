// Pokemon Data and Functions
#include "pokemon.h"
#include <stdlib.h> // For malloc/free
#include <string.h> // For strcpy

// Pokemon roster - add your CSV data here
static const PokemonSpecies pokemon_roster[] = {
    // Name, Type, HP, Attack, Defense, Speed
    {"Null", TYPE_NULL, 0, 0, 0, 0},
    {"Bulbasaur", TYPE_GRASS | TYPE_POISON, 45, 49, 49, 45},
    {"Ivysaur", TYPE_GRASS | TYPE_POISON, 60, 62, 63, 60},
    {"Venusaur", TYPE_GRASS | TYPE_POISON, 80, 82, 83, 80},
    {"Charmander", TYPE_FIRE, 39, 52, 43, 65},
    {"Charmeleon", TYPE_FIRE, 58, 64, 58, 80},
    {"Charizard", TYPE_FIRE | TYPE_FLYING, 78, 84, 78, 100},
    {"Squirtle", TYPE_WATER, 44, 48, 65, 43},
    {"Wartortle", TYPE_WATER, 59, 63, 80, 58},
    {"Blastoise", TYPE_WATER, 79, 83, 100, 78},
    {"Caterpie", TYPE_BUG, 45, 30, 35, 45},
    {"Metapod", TYPE_BUG, 50, 20, 55, 30},
    {"Butterfree", TYPE_BUG, 60, 45, 50, 70},
    {"Weedle", TYPE_BUG, 40, 35, 30, 50},
    {"Kakuna", TYPE_BUG, 45, 25, 50, 35},
    {"Beedrill", TYPE_BUG, 65, 90, 40, 75},
    {"Pidgey", TYPE_NORMAL, 40, 45, 40, 56},
    {"Pidgeotto", TYPE_NORMAL, 63, 60, 55, 71},
    {"Pidgeot", TYPE_NORMAL, 83, 80, 75, 101},
    {"Rattata", TYPE_NORMAL, 30, 56, 35, 72},
    {"Raticate", TYPE_NORMAL, 55, 81, 60, 97},
    {"Spearow", TYPE_NORMAL | TYPE_FLYING, 40, 60, 30, 70},
    {"Fearow", TYPE_NORMAL | TYPE_FLYING, 65, 90, 65, 100},
    {"Ekans", TYPE_POISON, 35, 60, 44, 55},
    {"Arbok", TYPE_POISON, 60, 85, 69, 80},
    {"Pikachu", TYPE_ELECTRIC, 35, 55, 40, 90},
    {"Raichu", TYPE_ELECTRIC, 60, 90, 55, 110},
    {"Sandshrew", TYPE_GROUND, 50, 75, 85, 40},
    {"Sandslash", TYPE_GROUND, 75, 100, 110, 65},
    {"Nidoran♀", TYPE_POISON, 55, 47, 52, 41},
    {"Nidorina", TYPE_POISON, 70, 62, 67, 56},
    {"Nidoqueen", TYPE_POISON | TYPE_GROUND, 90, 82, 87, 76},
    {"Nidoran♂", TYPE_POISON, 46, 57, 40, 50},
    {"Nidorino", TYPE_POISON, 61, 72, 57, 65},
    {"Nidoking", TYPE_POISON | TYPE_GROUND, 81, 102, 77, 85},
    {"Clefairy", TYPE_FAIRY, 70, 45, 48, 35},
    {"Clefable", TYPE_FAIRY, 95, 70, 73, 60},
    {"Vulpix", TYPE_FIRE, 38, 41, 40, 65},
    {"Ninetales", TYPE_FIRE, 73, 76, 75, 100},
    {"Jigglypuff", TYPE_NORMAL | TYPE_FAIRY, 115, 45, 20, 20},
    {"Wigglytuff", TYPE_NORMAL | TYPE_FAIRY, 140, 70, 45, 45},
    {"Zubat", TYPE_POISON | TYPE_FLYING, 40, 45, 35, 55},
    {"Golbat", TYPE_POISON | TYPE_FLYING, 75, 80, 70, 90},
    {"Oddish", TYPE_GRASS | TYPE_POISON, 45, 50, 55, 30},
    {"Gloom", TYPE_GRASS | TYPE_POISON, 60, 65, 70, 40},
    {"Vileplume", TYPE_GRASS | TYPE_POISON, 75, 80, 85, 50},
    {"Paras", TYPE_BUG | TYPE_GRASS, 35, 70, 55, 25},
    {"Parasect", TYPE_BUG | TYPE_GRASS, 60, 95, 80, 30},
    {"Venonat", TYPE_BUG | TYPE_POISON, 60, 55, 50, 45},
    {"Venomoth", TYPE_BUG | TYPE_POISON, 70, 65, 60, 90},
    {"Diglett", TYPE_GROUND, 10, 55, 25, 95},
    {"Dugtrio", TYPE_GROUND, 35, 80, 50, 120},
    {"Meowth", TYPE_NORMAL, 40, 45, 35, 90},
    {"Persian", TYPE_NORMAL, 65, 70, 60, 115},
    {"Psyduck", TYPE_WATER, 50, 52, 48, 55},
    {"Golduck", TYPE_WATER, 80, 82, 78, 85},
    {"Mankey", TYPE_FIGHTING, 40, 80, 35, 70},
    {"Primeape", TYPE_FIGHTING, 65, 105, 60, 95},
    {"Growlithe", TYPE_FIRE, 55, 70, 45, 60},
    {"Arcanine", TYPE_FIRE, 90, 110, 80, 95},
    {"Poliwag", TYPE_WATER, 40, 50, 40, 90},
    {"Poliwhirl", TYPE_WATER, 65, 65, 65, 90},
    {"Poliwrath", TYPE_WATER | TYPE_FIGHTING, 90, 95, 95, 70},
    {"Abra", TYPE_PSYCHIC, 25, 20, 15, 90},
    {"Kadabra", TYPE_PSYCHIC, 40, 35, 30, 105},
    {"Alakazam", TYPE_PSYCHIC, 55, 50, 45, 120},
    {"Machop", TYPE_FIGHTING, 70, 80, 50, 35},
    {"Machoke", TYPE_FIGHTING, 80, 100, 70, 45},
    {"Machamp", TYPE_FIGHTING, 90, 130, 80, 55},
    {"Bellsprout", TYPE_GRASS | TYPE_POISON, 50, 75, 35, 40},
    {"Weepinbell", TYPE_GRASS | TYPE_POISON, 65, 90, 50, 55},
    {"Victreebel", TYPE_GRASS | TYPE_POISON, 80, 105, 65, 70},
    {"Tentacool", TYPE_WATER | TYPE_POISON, 40, 40, 35, 70},
    {"Tentacruel", TYPE_WATER | TYPE_POISON, 80, 70, 65, 100},
    {"Geodude", TYPE_ROCK | TYPE_GROUND, 40, 80, 100, 20},
    {"Graveler", TYPE_ROCK | TYPE_GROUND, 55, 95, 115, 35},
    {"Golem", TYPE_ROCK | TYPE_GROUND, 80, 120, 130, 45},
    {"Ponyta", TYPE_FIRE, 50, 85, 55, 90},
    {"Rapidash", TYPE_FIRE, 65, 100, 70, 105},
    {"Slowpoke", TYPE_WATER | TYPE_PSYCHIC, 90, 65, 65, 15},
    {"Slowbro", TYPE_WATER | TYPE_PSYCHIC, 95, 75, 110, 30},
    {"Magnemite", TYPE_ELECTRIC | TYPE_STEEL, 25, 35, 70, 45},
    {"Magneton", TYPE_ELECTRIC | TYPE_STEEL, 50, 60, 95, 70},
    {"Farfetch'd", TYPE_NORMAL | TYPE_FLYING, 52, 65, 55, 60},
    {"Doduo", TYPE_NORMAL | TYPE_FLYING, 35, 85, 45, 75},
    {"Dodrio", TYPE_NORMAL | TYPE_FLYING, 60, 110, 70, 100},
    {"Seel", TYPE_WATER, 65, 45, 55, 45},
    {"Dewgong", TYPE_WATER | TYPE_ICE, 90, 70, 80, 70},
    {"Grimer", TYPE_POISON, 80, 80, 50, 25},
    {"Muk", TYPE_POISON, 105, 105, 75, 50},
    {"Shellder", TYPE_WATER, 30, 65, 100, 40},
    {"Cloyster", TYPE_WATER | TYPE_ICE, 50, 95, 180, 70},
    {"Gastly", TYPE_GHOST | TYPE_POISON, 30, 35, 30, 80},
    {"Haunter", TYPE_GHOST | TYPE_POISON, 45, 50, 45, 95},
    {"Gengar", TYPE_GHOST | TYPE_POISON, 60, 65, 60, 110},
    {"Onix", TYPE_ROCK | TYPE_GROUND, 35, 45, 160, 70},
    {"Drowzee", TYPE_PSYCHIC, 60, 48, 45, 42},
    {"Hypno", TYPE_PSYCHIC, 85, 73, 70, 67},
    {"Krabby", TYPE_WATER, 30, 105, 90, 50},
    {"Kingler", TYPE_WATER, 55, 130, 115, 75},
    {"Voltorb", TYPE_ELECTRIC, 40, 30, 50, 100},
    {"Electrode", TYPE_ELECTRIC, 60, 50, 70, 140},
    {"Exeggcute", TYPE_GRASS | TYPE_PSYCHIC, 60, 40, 80, 40},
    {"Exeggutor", TYPE_GRASS | TYPE_PSYCHIC, 95, 95, 85, 55},
    {"Cubone", TYPE_GROUND, 50, 50, 95, 35},
    {"Marowak", TYPE_GROUND, 60, 80, 110, 45},
    {"Hitmonlee", TYPE_FIGHTING, 50, 120, 53, 87},
    {"Hitmonchan", TYPE_FIGHTING, 50, 105, 79, 76},
    {"Lickitung", TYPE_NORMAL, 90, 55, 75, 30},
    {"Koffing", TYPE_POISON, 40, 65, 95, 35},
    {"Weezing", TYPE_POISON, 65, 90, 120, 60},
    {"Rhyhorn", TYPE_GROUND | TYPE_ROCK, 80, 85, 95, 25},
    {"Rhydon", TYPE_GROUND | TYPE_ROCK, 105, 130, 120, 40},
    {"Chansey", TYPE_NORMAL, 250, 5, 5, 50},
    {"Tangela", TYPE_GRASS, 65, 55, 115, 60},
    {"Kangaskhan", TYPE_NORMAL, 105, 95, 80, 90},
    {"Horsea", TYPE_WATER, 30, 40, 70, 60},
    {"Seadra", TYPE_WATER, 55, 65, 95, 85},
    {"Goldeen", TYPE_WATER, 45, 67, 60, 63},
    {"Seaking", TYPE_WATER, 80, 92, 65, 68},
    {"Staryu", TYPE_WATER, 30, 45, 55, 85},
    {"Starmie", TYPE_WATER | TYPE_PSYCHIC, 60, 75, 85, 115},
    {"Mr. Mime", TYPE_PSYCHIC | TYPE_FAIRY, 40, 45, 65, 90},
    {"Scyther", TYPE_BUG | TYPE_FLYING, 70, 110, 80, 105},
    {"Jynx", TYPE_ICE | TYPE_PSYCHIC, 65, 50, 35, 95},
    {"Electabuzz", TYPE_ELECTRIC, 65, 83, 57, 105},
    {"Magmar", TYPE_FIRE, 65, 95, 57, 93},
    {"Pinsir", TYPE_BUG, 65, 125, 100, 85},
    {"Tauros", TYPE_NORMAL, 75, 100, 95, 110},
    {"Magikarp", TYPE_WATER, 20, 10, 55, 80},
    {"Gyarados", TYPE_WATER | TYPE_FLYING, 95, 125, 79, 81},
    {"Lapras", TYPE_WATER | TYPE_ICE, 130, 85, 80, 60},
    {"Ditto", TYPE_NORMAL, 48, 48, 48, 48},
    {"Eevee", TYPE_NORMAL, 55, 55, 50, 55},
    {"Vaporeon", TYPE_WATER, 130, 65, 60, 65},
    {"Jolteon", TYPE_ELECTRIC, 65, 65, 60, 130},
    {"Flareon", TYPE_FIRE, 65, 130, 60, 65},
    {"Porygon", TYPE_NORMAL, 65, 60, 70, 40},
    {"Omanyte", TYPE_ROCK | TYPE_WATER, 35, 40, 100, 35},
    {"Omastar", TYPE_ROCK | TYPE_WATER, 70, 60, 125, 55},
    {"Kabuto", TYPE_ROCK | TYPE_WATER, 30, 80, 90, 55},
    {"Kabutops", TYPE_ROCK | TYPE_WATER, 60, 115, 105, 80},
    {"Aerodactyl", TYPE_ROCK | TYPE_FLYING, 80, 105, 65, 130},
    {"Snorlax", TYPE_NORMAL, 160, 110, 65, 30},
    {"Articuno", TYPE_ICE | TYPE_FLYING, 90, 85, 100, 85},
    {"Zapdos", TYPE_ELECTRIC | TYPE_FLYING, 90, 90, 85, 100},
    {"Moltres", TYPE_FIRE | TYPE_FLYING, 90, 100, 90, 90},
    {"Dratini", TYPE_DRAGON, 41, 64, 45, 50},
    {"Dragonair", TYPE_DRAGON, 61, 84, 65, 70},
    {"Dragonite", TYPE_DRAGON | TYPE_FLYING, 91, 134, 95, 80},
    {"Mewtwo", TYPE_PSYCHIC, 106, 110, 90, 130},
    {"Mew", TYPE_PSYCHIC, 100, 100, 100, 100}
    // Add more from your CSV...
};

const PokemonSpecies* pokemon_get_species_list(void) {
    return pokemon_roster;
}

int pokemon_get_species_count(void) {
    return sizeof(pokemon_roster) / sizeof(pokemon_roster[0]);
}

Pokemon* pokemon_create_from_species(const PokemonSpecies* species, uint8_t level, int pokedexId) {
    Pokemon* p = malloc(sizeof(Pokemon));
    if(!p) return NULL;

    strncpy(p->name, species->name, 15);
    p->name[15] = '\0';

    p->type = species->type;
    p->level = level;
    p->pokedexId = (uint8_t)pokedexId;

    // Calculate stats based on base stats and level
    p->max_hp = species->base_hp + (level * 2);
    p->current_hp = p->max_hp;
    p->attack = species->base_attack + level;
    p->defense = species->base_defense + level;
    p->speed = species->base_speed + level;

    // Assign moves based on type
    if(species->type == TYPE_FIRE) {
        p->moves[0] = (Move){"Tackle", 35, TYPE_NORMAL, 95, 35};
        p->moves[1] = (Move){"Ember", 40, TYPE_FIRE, 100, 25};
        p->moves[2] = (Move){"Growl", 0, TYPE_NORMAL, 100, 40};
        p->moves[3] = (Move){"Fire Spin", 35, TYPE_FIRE, 85, 15};
    } else if(species->type == TYPE_WATER) {
        p->moves[0] = (Move){"Tackle", 35, TYPE_NORMAL, 95, 35};
        p->moves[1] = (Move){"Water Gun", 40, TYPE_WATER, 100, 25};
        p->moves[2] = (Move){"Tail Whip", 0, TYPE_NORMAL, 100, 40};
        p->moves[3] = (Move){"Bubble", 40, TYPE_WATER, 100, 30};
    } else if(species->type == TYPE_GRASS) {
        p->moves[0] = (Move){"Tackle", 35, TYPE_NORMAL, 95, 35};
        p->moves[1] = (Move){"Vine Whip", 45, TYPE_GRASS, 100, 25};
        p->moves[2] = (Move){"Growl", 0, TYPE_NORMAL, 100, 40};
        p->moves[3] = (Move){"Razor Leaf", 55, TYPE_GRASS, 95, 25};
    } else if(species->type == TYPE_ELECTRIC) {
        p->moves[0] = (Move){"Quick Attack", 40, TYPE_NORMAL, 100, 30};
        p->moves[1] = (Move){"Thunder Shock", 40, TYPE_ELECTRIC, 100, 30};
        p->moves[2] = (Move){"Growl", 0, TYPE_NORMAL, 100, 40};
        p->moves[3] = (Move){"Thunderbolt", 90, TYPE_ELECTRIC, 100, 15};
    } else {
        // Default moves for other types
        p->moves[0] = (Move){"Tackle", 35, TYPE_NORMAL, 95, 35};
        p->moves[1] = (Move){"Scratch", 40, TYPE_NORMAL, 100, 35};
        p->moves[2] = (Move){"Growl", 0, TYPE_NORMAL, 100, 40};
        p->moves[3] = (Move){"Take Down", 90, TYPE_NORMAL, 85, 20};
    }

    return p;
}

// Static means "only visible in this file"
static const uint8_t base_stats[TYPE_COUNT][3] = {
    // HP, ATK, DEF
    {40, 50, 40}, // Normal
    {38, 52, 43}, // Fire
    {44, 48, 65}, // Water
    {45, 49, 49}, // Grass
    {35, 55, 40}, // Electric
};

Pokemon* pokemon_create(const char* name, PokemonType type, uint8_t level) {
    // Allocate memory
    Pokemon* p = malloc(sizeof(Pokemon));
    if(!p) return NULL; // Always check malloc success

    // Copy name (safe string copy)
    strncpy(p->name, name, 15);
    p->name[15] = '\0'; // Ensure null termination

    // Set stats based on type and level
    // Set all stats
    p->type = type;
    p->level = level;
    p->pokedexId = 0;
    p->max_hp = base_stats[type][0] + (level * 2);
    p->current_hp = p->max_hp;
    p->attack = base_stats[type][1] + level;
    p->defense = base_stats[type][2] + level;
    p->speed = 45 + level;

    return p;
}

// Calculate damage from one Pokemon to another
uint16_t pokemon_calculate_damage(Pokemon* attacker, Pokemon* defender, uint8_t move_power) {
    // Simple damage formula
    float damage = ((2.0f * attacker->level / 5.0f + 2.0f) * move_power * attacker->attack /
                    defender->defense) /
                       50.0f +
                   2.0f;

    // Type effectiveness
    if((attacker->type == TYPE_FIRE && defender->type == TYPE_GRASS) ||
       (attacker->type == TYPE_WATER && defender->type == TYPE_FIRE) ||
       (attacker->type == TYPE_GRASS && defender->type == TYPE_WATER)) {
        damage *= 2.0f; // Super effective
    }

    return (uint16_t)damage;
}

void pokemon_free(Pokemon* pokemon) {
    free(pokemon); // Simple - just free the memory
}
