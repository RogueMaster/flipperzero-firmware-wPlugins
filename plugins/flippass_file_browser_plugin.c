#include "flippass_file_browser_plugin.h"
#include "../scenes/flippass_scene_file_browser.h"
#include "../flippass.h"

static const FlipPassFileBrowserPluginV1 api = {
    .api_version = FLIPPASS_FILE_BROWSER_PLUGIN_API_VERSION,
    .on_enter = flippass_scene_file_browser_on_enter,
    .on_event = flippass_scene_file_browser_on_event,
    .on_exit = flippass_scene_file_browser_on_exit,
    .check_database = flippass_file_browser_check_database,
};
static const FlipperAppPluginDescriptor descriptor = {
    .appid = FLIPPASS_FILE_BROWSER_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_FILE_BROWSER_PLUGIN_API_VERSION,
    .entry_point = &api,
};
const FlipperAppPluginDescriptor* flippass_file_browser_plugin_ep(void) {
    return &descriptor;
}
