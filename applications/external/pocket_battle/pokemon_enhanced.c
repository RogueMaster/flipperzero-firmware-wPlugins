// Enhanced Pokemon Implementation with pokeyellow data
#include "pokemon_enhanced.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Type effectiveness chart (simplified)
static const float type_chart[TYPE_COUNT][TYPE_COUNT] = {
    // Defending type ->
    // NOR  FIR  WAT  GRA  ELE  FIG  POI  GRO  FLY  PSY  BUG  ROC  GHO  DRA  DAR  STE  FAI  ICE
    {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 0.0, 1.0, 1.0, 0.5, 1.0, 1.0}, // NORMAL
    {1.0, 0.5, 0.5, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 0.5, 1.0, 0.5, 1.0, 2.0, 1.0, 2.0}, // FIRE
    {1.0, 2.0, 0.5, 0.5, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 2.0, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0}, // WATER
    {1.0, 0.5, 2.0, 0.5, 1.0, 1.0, 0.5, 2.0, 0.5, 1.0, 0.5, 2.0, 1.0, 0.5, 1.0, 0.5, 1.0, 1.0}, // GRASS
    {1.0, 1.0, 2.0, 0.5, 0.5, 1.0, 1.0, 0.0, 2.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0}, // ELECTRIC
    {2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0, 0.5, 0.5, 0.5, 2.0, 0.0, 1.0, 2.0, 2.0, 0.5, 2.0}, // FIGHTING
    {1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 0.5, 0.5, 1.0, 1.0, 1.0, 0.5, 0.5, 1.0, 1.0, 0.0, 2.0, 1.0}, // POISON
    {1.0, 2.0, 1.0, 0.5, 2.0, 1.0, 2.0, 1.0, 0.0, 1.0, 0.5, 2.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0}, // GROUND
    {1.0, 1.0, 1.0, 2.0, 0.5, 2.0, 1.0, 1.0, 1.0, 1.0, 2.0, 0.5, 1.0, 1.0, 1.0, 0.5, 1.0, 1.0}, // FLYING
    {1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 1.0, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0, 0.0, 0.5, 1.0, 1.0}, // PSYCHIC
    {1.0, 0.5, 1.0, 2.0, 1.0, 0.5, 0.5, 1.0, 0.5, 2.0, 1.0, 1.0, 0.5, 1.0, 2.0, 0.5, 0.5, 1.0}, // BUG
    {1.0, 2.0, 1.0, 1.0, 1.0, 0.5, 1.0, 0.5, 2.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0, 2.0}, // ROCK
    {0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 2.0, 1.0, 0.5, 1.0, 1.0, 1.0}, // GHOST
    {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 0.5, 0.0, 1.0}, // DRAGON
    {1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 2.0, 1.0, 0.5, 1.0, 0.5, 1.0}, // DARK
    {1.0, 0.5, 0.5, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 0.5, 2.0, 2.0}, // STEEL
    {1.0, 0.5, 1.0, 1.0, 1.0, 2.0, 0.5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 0.5, 1.0, 1.0}, // FAIRY
    {1.0, 0.5, 0.5, 2.0, 1.0, 1.0, 1.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 0.5, 1.0, 0.5} // ICE
};

EnhancedPokemon* enhanced_pokemon_create(uint8_t pokedex_id, uint8_t level) {
    EnhancedPokemon* pokemon = malloc(sizeof(EnhancedPokemon));
    if(!pokemon) return NULL;

    // Get species data
    const PokemonSpecies* species_list = pokemon_get_yellow_species_list();
    int species_count = pokemon_get_yellow_species_count();

    if(pokedex_id >= species_count) {
        pokedex_id = 0; // Default to first Pokemon
    }

    const PokemonSpecies* species = &species_list[pokedex_id];

    // Initialize basic data
    strncpy(pokemon->name, species->name, sizeof(pokemon->name) - 1);
    pokemon->name[sizeof(pokemon->name) - 1] = '\0';

    pokemon->type1 = species->type;
    pokemon->type2 = TYPE_NULL; // Will be enhanced later for dual types
    pokemon->level = level;
    pokemon->pokedex_id = pokedex_id;

    // Set base stats
    pokemon->base_hp = species->base_hp;
    pokemon->base_attack = species->base_attack;
    pokemon->base_defense = species->base_defense;
    pokemon->base_speed = species->base_speed;
    pokemon->base_special = 50; // Default special stat

    // Generate random IVs (0-15 in original games)
    pokemon->iv_hp = rand() % 16;
    pokemon->iv_attack = rand() % 16;
    pokemon->iv_defense = rand() % 16;
    pokemon->iv_speed = rand() % 16;
    pokemon->iv_special = rand() % 16;

    // Calculate actual stats
    enhanced_pokemon_calculate_stats(pokemon);

    // Initialize status
    pokemon->status = STATUS_NONE;
    pokemon->status_turns = 0;

    // Initialize moves (simplified - just give basic moves)
    const Move* moves_list = pokemon_get_yellow_moves_list();
    for(int i = 0; i < 4; i++) {
        if(i < pokemon_get_yellow_moves_count()) {
            pokemon->moves[i] = moves_list[i];
            pokemon->move_pp[i] = moves_list[i].pp;
        } else {
            // Empty move slot
            pokemon->moves[i].name = "---";
            pokemon->moves[i].power = 0;
            pokemon->moves[i].type = TYPE_NULL;
            pokemon->moves[i].accuracy = 0;
            pokemon->moves[i].pp = 0;
            pokemon->move_pp[i] = 0;
        }
    }

    // Initialize experience
    pokemon->experience = level * level * level; // Simplified
    pokemon->growth_rate = GROWTH_MEDIUM_FAST;

    return pokemon;
}

void enhanced_pokemon_free(EnhancedPokemon* pokemon) {
    if(pokemon) {
        free(pokemon);
    }
}

void enhanced_pokemon_calculate_stats(EnhancedPokemon* pokemon) {
    // Pokemon stat calculation formula (simplified Gen 1)
    // Stat = (((Base + IV) * 2 + sqrt(EV)/4) * Level / 100) + 5
    // For simplicity, we'll ignore EVs and use a simpler formula

    uint8_t level = pokemon->level;

    // HP calculation is special
    pokemon->max_hp = ((pokemon->base_hp + pokemon->iv_hp) * 2 * level / 100) + level + 10;
    pokemon->current_hp = pokemon->max_hp;

    // Other stats
    pokemon->attack = ((pokemon->base_attack + pokemon->iv_attack) * 2 * level / 100) + 5;
    pokemon->defense = ((pokemon->base_defense + pokemon->iv_defense) * 2 * level / 100) + 5;
    pokemon->speed = ((pokemon->base_speed + pokemon->iv_speed) * 2 * level / 100) + 5;
    pokemon->special = ((pokemon->base_special + pokemon->iv_special) * 2 * level / 100) + 5;
}

float get_type_effectiveness(
    PokemonType attack_type,
    PokemonType defend_type1,
    PokemonType defend_type2) {
    if(attack_type >= TYPE_COUNT || defend_type1 >= TYPE_COUNT) {
        return 1.0f;
    }

    float effectiveness = type_chart[attack_type][defend_type1];

    if(defend_type2 != TYPE_NULL && defend_type2 < TYPE_COUNT) {
        effectiveness *= type_chart[attack_type][defend_type2];
    }

    return effectiveness;
}

uint16_t enhanced_pokemon_calculate_damage(
    EnhancedPokemon* attacker,
    EnhancedPokemon* defender,
    uint8_t move_index) {
    if(move_index >= 4 || attacker->moves[move_index].power == 0) {
        return 0;
    }

    Move* move = &attacker->moves[move_index];

    // Check if move hits
    if((rand() % 100) >= move->accuracy) {
        return 0; // Miss
    }

    // Basic damage calculation (simplified Gen 1 formula)
    uint16_t attack_stat = (move->type <= TYPE_PSYCHIC) ? attacker->attack : attacker->special;
    uint16_t defense_stat = (move->type <= TYPE_PSYCHIC) ? defender->defense : defender->special;

    // Base damage
    uint32_t damage =
        ((2 * attacker->level + 10) / 250.0) * (attack_stat / (float)defense_stat) * move->power +
        2;

    // Type effectiveness
    float effectiveness = get_type_effectiveness(move->type, defender->type1, defender->type2);
    damage = (uint32_t)(damage * effectiveness);

    // STAB (Same Type Attack Bonus)
    if(move->type == attacker->type1 || move->type == attacker->type2) {
        damage = (uint32_t)(damage * 1.5);
    }

    // Random factor (85-100%)
    damage = (uint32_t)(damage * (85 + rand() % 16) / 100.0);

    // Critical hit (1/16 chance)
    if((rand() % 16) == 0) {
        damage *= 2;
    }

    return (uint16_t)damage;
}

void enhanced_pokemon_apply_status(EnhancedPokemon* pokemon, uint8_t status) {
    if(pokemon->status == STATUS_NONE) {
        pokemon->status = status;
        pokemon->status_turns = 0;
    }
}

bool enhanced_pokemon_can_move(EnhancedPokemon* pokemon) {
    // Check if Pokemon can move based on status
    switch(pokemon->status) {
    case STATUS_SLEEP:
        return (rand() % 4) == 0; // 25% chance to wake up
    case STATUS_FREEZE:
        return (rand() % 5) == 0; // 20% chance to thaw
    case STATUS_PARALYZE:
        return (rand() % 4) != 0; // 75% chance to move
    default:
        return true;
    }
}

void enhanced_pokemon_level_up(EnhancedPokemon* pokemon) {
    if(pokemon->level < 100) {
        pokemon->level++;
        enhanced_pokemon_calculate_stats(pokemon);
    }
}

// Battle system implementation
EnhancedBattle* enhanced_battle_create(EnhancedPokemon* player, EnhancedPokemon* enemy) {
    EnhancedBattle* battle = malloc(sizeof(EnhancedBattle));
    if(!battle) return NULL;

    battle->player_pokemon = player;
    battle->enemy_pokemon = enemy;
    battle->turn_count = 0;
    battle->battle_over = false;
    battle->player_won = false;
    strcpy(battle->battle_log, "Battle started!");

    return battle;
}

void enhanced_battle_free(EnhancedBattle* battle) {
    if(battle) {
        free(battle);
    }
}

bool enhanced_battle_execute_turn(EnhancedBattle* battle, uint8_t player_move, uint8_t enemy_move) {
    if(battle->battle_over) return false;

    battle->turn_count++;
    strcpy(battle->battle_log, "");

    // Determine move order based on speed
    bool player_first = battle->player_pokemon->speed >= battle->enemy_pokemon->speed;

    EnhancedPokemon* first = player_first ? battle->player_pokemon : battle->enemy_pokemon;
    EnhancedPokemon* second = player_first ? battle->enemy_pokemon : battle->player_pokemon;
    uint8_t first_move = player_first ? player_move : enemy_move;
    uint8_t second_move = player_first ? enemy_move : player_move;

    // First Pokemon attacks
    if(enhanced_pokemon_can_move(first)) {
        uint16_t damage = enhanced_pokemon_calculate_damage(first, second, first_move);
        if(damage > 0) {
            second->current_hp = (damage >= second->current_hp) ? 0 : second->current_hp - damage;
            snprintf(
                battle->battle_log + strlen(battle->battle_log),
                sizeof(battle->battle_log) - strlen(battle->battle_log),
                "%s used %s! Dealt %d damage. ",
                first->name,
                first->moves[first_move].name,
                damage);
        }
    }

    // Check if battle is over
    if(second->current_hp == 0) {
        battle->battle_over = true;
        battle->player_won = (second == battle->enemy_pokemon);
        return true;
    }

    // Second Pokemon attacks
    if(enhanced_pokemon_can_move(second)) {
        uint16_t damage = enhanced_pokemon_calculate_damage(second, first, second_move);
        if(damage > 0) {
            first->current_hp = (damage >= first->current_hp) ? 0 : first->current_hp - damage;
            snprintf(
                battle->battle_log + strlen(battle->battle_log),
                sizeof(battle->battle_log) - strlen(battle->battle_log),
                "%s used %s! Dealt %d damage. ",
                second->name,
                second->moves[second_move].name,
                damage);
        }
    }

    // Check if battle is over
    if(first->current_hp == 0) {
        battle->battle_over = true;
        battle->player_won = (first == battle->enemy_pokemon);
        return true;
    }

    return false;
}

const char* enhanced_battle_get_log(EnhancedBattle* battle) {
    return battle->battle_log;
}
