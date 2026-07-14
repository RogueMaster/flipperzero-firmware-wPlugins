#include <gui/icon.h>
#include <stdio.h>
#include "flipper_hero_icons.h"

#pragma once

static const int faithfulIndexes[] = {
    0,  1,  2,  3,  4,  6, 10, 14, // Weapons
    26, 27, 28, 30, // Orbitals
    37, 38, 42, 43, // Eagles
    44, 48, 51, // Backpacks
    57, 63, 68, 69, 71, // Mines/emplacements
    78, 79, 80 // Mission
};
static const int specialRoundIndexes[5][4] = {
    // { start index, count, start index (faithful), count (faithful) }
    {0, 93, 0, 26}, // None
    {25, 12, 8, 4}, // Orbital
    {37, 7, 12, 4}, // Eagle
    {44, 13, 16, 3}, // Backpack
    {0, 25, 0, 8} // Weapon
}; /*
static const int faithfulSpecialRoundIndexes[][] = {
    {0, 26}, // None
    {8,  4}, // Orbital
    {12, 4}, // Eagle
    {16, 3}, // Backpack
    { 0, 8}  // Weapon
};*/

void get_stratagem_name(char name_buf[32], int i);
Icon* get_stratagem_icon(int i, bool large_icon);
void get_stratagem_seq(char seq_buf[9], int i);
