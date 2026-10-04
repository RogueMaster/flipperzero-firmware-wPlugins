#pragma once

#include <cfw/game_menu.h>

#define GAME_MENU_PLUGIN_APP_ID      "GameMenu"
#define GAME_MENU_PLUGIN_API_VERSION 1
#define GAME_MENU_PLUGIN_PATH        EXT_PATH("apps_data/loader/plugins/game_menu.fal")

/* Bump the plugin API version when this interface or its callback contract changes. */
typedef struct {
    /* Synchronous enumeration. Paths are borrowed only for the duration of each callback.
     * The host copies entries and unloads the FAL as soon as this call returns. The plugin
     * must not retain the callback/context, start workers, or leave timers/callbacks active. */
    GameMenuSource (*load)(Storage* storage, GameMenuEntryCallback callback, void* context);
} GameMenuPlugin;
