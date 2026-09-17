#ifndef __constants_h__
#define __constants_h__

#include <storage/storage.h> // For APP_DATA_PATH
#include "tuning.h"

/* Delay between two background activities performed
 * by secondary_thread */
#define BACKGROUND_ACTIVITY_TICKS 1000U

/* How many ticks a popup animation should stay
 * on the screen */
#define ANIMATION_TICKS_DURATION 1U

/* Strings */
#define LOG_TAG "Dragotchi"
static const char ABOUT_TEXT[] = "Dragotchi\n"
                                 "Raise a dragon\n"
                                 "White = well raised,\n"
                                 "black = neglected.\n"
                                 "\n"
                                 "GPLv3. Fork of MrModd's\n"
                                 "Matagotchi. Thanks MrModd!\n"
                                 "github.com/MrModd/Matagotchi";

static const char LIFE_STAGE_STRING[][10] = {"Egg",
                                             "Hatchling",
                                             "Wyrmling",
                                             "Drake",
                                             "Adult",
                                             "Dead"};
static const char ALIGNMENT_STRING[][6] = {"", "White", "Grey", "Black"};

/* Game state file info */
#define GAME_STATE_STORAGE_STATE_FILENAME "dragotchi.save"
#define GAME_STATE_STORAGE_SETTINGS_FILENAME "dragotchi.settings"
#define GAME_STATE_STORAGE_STATE_PATH APP_DATA_PATH(GAME_STATE_STORAGE_STATE_FILENAME)
#define GAME_STATE_STORAGE_SETTINGS_PATH APP_DATA_PATH(GAME_STATE_STORAGE_SETTINGS_FILENAME)
#define GAME_STATE_HEADER_MAGIC 0xDA
#define GAME_STATE_HEADER_VERSION 0x01

/* App version, shown in the menu header. Keep in sync with application.fam.
 */
#define DRAGOTCHI_VERSION "0.4.0"

#endif
