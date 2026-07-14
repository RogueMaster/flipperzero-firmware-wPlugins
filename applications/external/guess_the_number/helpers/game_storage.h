#pragma once

#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format_i.h>
#include "../guess_the_number.h"

#define GAME_SETTINGS_FILE_VERSION      1
#define CONFIG_FILE_DIRECTORY_PATH      EXT_PATH("apps_data/guess_the_number")
#define GAME_SETTINGS_SAVE_PATH         CONFIG_FILE_DIRECTORY_PATH "/guess_the_number.conf"
#define GAME_SETTINGS_SAVE_PATH_TMP     GAME_SETTINGS_SAVE_PATH ".tmp"
#define GAME_SETTINGS_HEADER            "GTN Config File"
#define GAME_SETTINGS_KEY_HAPTIC        "Haptic"
#define GAME_SETTINGS_KEY_LED           "Led"
#define GAME_SETTINGS_KEY_SPEAKER       "Speaker"
#define GAME_SETTINGS_KEY_SAVE_SETTINGS "SaveSettings"

#define GAME_SCORE_FILE_VERSION 1
#define GAME_SCORE_SAVE_PATH    CONFIG_FILE_DIRECTORY_PATH "/best_score.conf"
#define GAME_SCORE_HEADER       "GTN Score File"
#define GAME_SCORE_KEY_BEST     "BestScore"

void game_save_settings(void* context);
void game_read_settings(void* context);

void game_save_best_score(int best_score);
int game_read_best_score(void);
void game_clear_best_score(void);
