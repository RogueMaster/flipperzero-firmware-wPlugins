#pragma once

#include <storage/storage.h>

#define GAMEMENU_APPS_PATH   INT_PATH(".gamemenu_apps.txt")
#define GAMEMENU_LEGACY_PATH EXT_PATH(".config/cfw_gamesmenu.txt")
#define GAMEMENU_GAMES_PATH  EXT_PATH("apps/Games")

typedef enum {
    GameMenuSourceDefaults,
    GameMenuSourceCustom,
    GameMenuSourceLegacy,
} GameMenuSource;

/* The path is borrowed and valid only during the callback. */
typedef void (*GameMenuEntryCallback)(const char* path, void* context);

/* Defaults discover installed Games, GPIO/Games and GPIO/VGM FAPs, including subfolders. */
GameMenuSource game_menu_load(Storage* storage, GameMenuEntryCallback callback, void* context);

/* Save a custom list, including an intentionally empty list. */
bool game_menu_save(Storage* storage, const char* const* paths, size_t count);

/* Restore automatic discovery without modifying the historical SD-card configuration. */
bool game_menu_reset(Storage* storage);
