#include <loader/game_menu_plugin.h>
#include <flipper_application/flipper_application.h>

/* Use the same discovery/configuration implementation as the external CFW Settings app.
 * Only load() is reachable from this FAL; save/reset remain in the settings FAP. */
#include <cfw/game_menu.c>

static const GameMenuPlugin game_menu_plugin = {
    .load = game_menu_load,
};

static const FlipperAppPluginDescriptor game_menu_plugin_descriptor = {
    .appid = GAME_MENU_PLUGIN_APP_ID,
    .ep_api_version = GAME_MENU_PLUGIN_API_VERSION,
    .entry_point = &game_menu_plugin,
};

const FlipperAppPluginDescriptor* game_menu_ep(void) {
    return &game_menu_plugin_descriptor;
}
