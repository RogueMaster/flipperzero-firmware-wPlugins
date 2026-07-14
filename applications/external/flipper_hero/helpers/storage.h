#include "../data/data.h"

#pragma once
#define GAME_RECORDS_DIRECTORY "/ext/apps_data/flipper_hero"
#define GAME_RECORDS_FILE_PATH GAME_RECORDS_DIRECTORY "/flipper_hero.save"
#define GAME_CONFIG_FILE_PATH  GAME_RECORDS_DIRECTORY "/flipper_hero_config.save"

void save_game_config(Config* config);
bool load_game_config(Config* config);

void save_game_records(Record* record);
bool load_game_records(Record* record);
