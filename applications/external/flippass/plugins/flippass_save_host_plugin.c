#include "flippass_save_host_plugin.h"

static const FlipPassSaveHostPluginV1 flippass_save_host_plugin = {
    .api_version = FLIPPASS_SAVE_HOST_PLUGIN_API_VERSION,
    .current_database = flippass_save_host_current_database,
    .execute = flippass_save_host_execute,
#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
    .debug_after_open = flippass_save_host_debug_after_open,
#else
    .debug_after_open = NULL,
#endif
};

static const FlipperAppPluginDescriptor flippass_save_host_descriptor = {
    .appid = FLIPPASS_SAVE_HOST_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_SAVE_HOST_PLUGIN_API_VERSION,
    .entry_point = &flippass_save_host_plugin,
};

const FlipperAppPluginDescriptor* flippass_save_host_plugin_ep(void) {
    return &flippass_save_host_descriptor;
}
