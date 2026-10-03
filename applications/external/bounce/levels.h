#pragma once

/*
 * Bounce level maps. One char per 8x8 tile:
 *   '#' brick            'S' start position
 *   'O' ring (2 tiles tall: tile below must be empty)
 *   'o' ring sunk in water (tile below must be '~')
 *   'E' exit door segment (solid until every ring is collected)
 *   'C' checkpoint       '+' extra life
 *   '^' floor spike      'v' ceiling spike
 *   'J' spring pad       '~' water
 *   'M' spiker moving left/right, 'N' spiker moving up/down
 *   'm' / 'n' same spikers, starting in water
 * All rows of a level must have the same length.
 */

#include <stdint.h>

typedef struct {
    const char* name;
    uint8_t height;
    const char* const* rows;
} LevelDef;

/* 1. First Bounce */
static const char* const level1_rows[] = {
    "########################################",
    "#                                     E#",
    "#                                     E#",
    "#             O                       E#",
    "#                             O       E#",
    "#      O                 ##           E#",
    "#S               ##      ##           E#",
    "########################################",
};

/* 2. Thorns */
static const char* const level2_rows[] = {
    "########################################################",
    "#                                                     E#",
    "#                                                     E#",
    "#            O                       O                E#",
    "#                        C                            E#",
    "#     O                 ###    O              O       E#",
    "#S          ^^^        #####             ^^       ^^  E#",
    "########################################################",
};

/* 3. Spring Tower */
static const char* const level3_rows[] = {
    "############################################################",
    "#                    +                                    E#",
    "#                  O                                      E#",
    "#                                                         E#",
    "#                ######                     O             E#",
    "#                #    #                                   E#",
    "#         O      #    #      C                            E#",
    "#                #    #    #####~~~~~~~~~~~#####          E#",
    "#S         J    J#    # J  #####~~~~~~~~~~~#####    ^^    E#",
    "############################################################",
};

/* 4. Spikers */
static const char* const level4_rows[] = {
    "################################################################",
    "#                    N                               N        E#",
    "#                                +                            E#",
    "#           O                                           O     E#",
    "#                               #                             E#",
    "#               ##      O      ###           ##               E#",
    "#S     #   M    ##          C #####     M    ##   N           E#",
    "################################################################",
};

/* 5. The Climb */
static const char* const level5_rows[] = {
    "####################",
    "#E      O          #",
    "#E           +     #",
    "################   #",
    "#                  #",
    "#         O        #",
    "#    C      ^^     #",
    "#   #############J##",
    "#                  #",
    "#        O         #",
    "#     ^^           #",
    "##J#############   #",
    "#                  #",
    "#       O          #",
    "#S                 #",
    "#################J##",
};

/* 6. Deep Water */
static const char* const level6_rows[] = {
    "############################################################",
    "#                                                         E#",
    "#                                            O            E#",
    "#                                                         E#",
    "#S                                                        E#",
    "####                         C              ####          E#",
    "####~~~~~~~~~~~~n~~~~~~~~~~#####~~~~~~~~~~~~####          E#",
    "####~~~~o~~~~~~~~~~~~~~~~~~#####~~~~~~o~~~~~####          E#",
    "####~~~~~~~~~~~~~~~~~~~~~~~#####~~~~~~~~~~~~####    ^^    E#",
    "#################################################J##########",
};

/* 7. Spike Alley */
static const char* const level7_rows[] = {
    "########################################################################",
    "#                     ############                              O     E#",
    "#                     ############                                    E#",
    "#       O             ############                                    E#",
    "#                     ############                                    E#",
    "#                        v O v                                        E#",
    "#S     ^^     ^^                    C  ^^^##^^^     #   M   #         E#",
    "################################################################J#######",
};

/* 8. Grand Finale */
static const char* const level8_rows[] = {
    "################################################################################################",
    "#                                                                           N                 E#",
    "#                                                                                             E#",
    "#                                                                                             E#",
    "#                                                                                             E#",
    "#                                  O                                                          E#",
    "#                                                               O                             E#",
    "#    O                        ############                                                    E#",
    "#            #~~~~~~~~n~~#       vv      #                                                    E#",
    "#          ###~~~~~o~~~~~#               #                   ###  ###               O         E#",
    "# S  ^^  #####~~~~~~~~~~~#            +  #    C #   M    # ^^^^^^^^^^^^         N             E#",
    "############################J###############J###################################################",
};

static const LevelDef levels[] = {
    {"First Bounce", 8, level1_rows},
    {"Thorns", 8, level2_rows},
    {"Spring Tower", 10, level3_rows},
    {"Spikers", 8, level4_rows},
    {"The Climb", 16, level5_rows},
    {"Deep Water", 10, level6_rows},
    {"Spike Alley", 8, level7_rows},
    {"Grand Finale", 12, level8_rows},
};

#define LEVEL_COUNT (sizeof(levels) / sizeof(levels[0]))
