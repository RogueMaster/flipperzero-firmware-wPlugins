// Manual moves data based on pokeyellow
#include "pokemon.h"

// Moves data extracted from pokeyellow (manually curated)
static const Move moves_yellow[] = {
    {"Pound", 40, TYPE_NORMAL, 100, 35},
    {"Karate Chop", 50, TYPE_FIGHTING, 100, 25},
    {"Double Slap", 15, TYPE_NORMAL, 85, 10},
    {"Comet Punch", 18, TYPE_NORMAL, 85, 15},
    {"Mega Punch", 80, TYPE_NORMAL, 85, 20},
    {"Pay Day", 40, TYPE_NORMAL, 100, 20},
    {"Fire Punch", 75, TYPE_FIRE, 100, 15},
    {"Ice Punch", 75, TYPE_ICE, 100, 15},
    {"Thunder Punch", 75, TYPE_ELECTRIC, 100, 15},
    {"Scratch", 40, TYPE_NORMAL, 100, 35},
    {"Vice Grip", 55, TYPE_NORMAL, 100, 30},
    {"Guillotine", 1, TYPE_NORMAL, 30, 5},
    {"Razor Wind", 80, TYPE_NORMAL, 75, 10},
    {"Swords Dance", 0, TYPE_NORMAL, 100, 30},
    {"Cut", 50, TYPE_NORMAL, 95, 30},
    {"Gust", 40, TYPE_FLYING, 100, 35},
    {"Wing Attack", 35, TYPE_FLYING, 100, 35},
    {"Whirlwind", 0, TYPE_NORMAL, 85, 20},
    {"Fly", 70, TYPE_FLYING, 95, 15},
    {"Bind", 15, TYPE_NORMAL, 75, 20},
    {"Slam", 80, TYPE_NORMAL, 75, 20},
    {"Vine Whip", 35, TYPE_GRASS, 100, 10},
    {"Stomp", 65, TYPE_NORMAL, 100, 20},
    {"Double Kick", 30, TYPE_FIGHTING, 100, 30},
    {"Mega Kick", 120, TYPE_NORMAL, 75, 5},
    {"Jump Kick", 70, TYPE_FIGHTING, 95, 25},
    {"Rolling Kick", 60, TYPE_FIGHTING, 85, 15},
    {"Sand Attack", 0, TYPE_GROUND, 100, 15},
    {"Headbutt", 70, TYPE_NORMAL, 100, 15},
    {"Horn Attack", 65, TYPE_NORMAL, 100, 25},
    {"Fury Attack", 15, TYPE_NORMAL, 85, 20},
    {"Horn Drill", 1, TYPE_NORMAL, 30, 5},
    {"Tackle", 35, TYPE_NORMAL, 95, 35},
    {"Body Slam", 85, TYPE_NORMAL, 100, 15},
    {"Wrap", 15, TYPE_NORMAL, 85, 20},
    {"Take Down", 90, TYPE_NORMAL, 85, 20},
    {"Thrash", 90, TYPE_NORMAL, 100, 20},
    {"Double Edge", 100, TYPE_NORMAL, 100, 15},
    {"Tail Whip", 0, TYPE_NORMAL, 100, 30},
    {"Poison Sting", 15, TYPE_POISON, 100, 35},
    {"Twineedle", 25, TYPE_BUG, 100, 20},
    {"Pin Missile", 14, TYPE_BUG, 85, 20},
    {"Leer", 0, TYPE_NORMAL, 100, 30},
    {"Bite", 60, TYPE_DARK, 100, 25},
    {"Growl", 0, TYPE_NORMAL, 100, 40},
    {"Roar", 0, TYPE_NORMAL, 100, 20},
    {"Sing", 0, TYPE_NORMAL, 55, 15},
    {"Supersonic", 0, TYPE_NORMAL, 55, 20},
    {"Sonic Boom", 20, TYPE_NORMAL, 90, 20},
    {"Disable", 0, TYPE_NORMAL, 55, 20}};

const Move* pokemon_get_yellow_moves_list(void) {
    return moves_yellow;
}

int pokemon_get_yellow_moves_count(void) {
    return sizeof(moves_yellow) / sizeof(moves_yellow[0]);
}
