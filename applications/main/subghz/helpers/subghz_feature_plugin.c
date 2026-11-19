#include "subghz_feature_plugin.h"

#include "../api/subghz_app_api_interface.h"
#include "../subghz_i.h"
#include <loader/firmware_api/firmware_api.h>

#define TAG "SubGhzFeaturePlugin"

PluginManager* subghz_feature_plugin_load(SubGhz* subghz, const char* app_id, const char* path) {
    furi_assert(!subghz->api_resolver);

    subghz->api_resolver = composite_api_resolver_alloc();
    composite_api_resolver_add(subghz->api_resolver, firmware_api_interface);
    composite_api_resolver_add(subghz->api_resolver, subghz_application_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        app_id,
        SUBGHZ_FEATURE_PLUGIN_API_VERSION,
        composite_api_resolver_get(subghz->api_resolver));
    PluginManagerError error = plugin_manager_load_single(manager, path);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load %s (error %d)", path, error);
        subghz_feature_plugin_unload(subghz, manager);
        return NULL;
    }

    return manager;
}

void subghz_feature_plugin_unload(SubGhz* subghz, PluginManager* manager) {
    if(manager) {
        plugin_manager_free(manager);
        composite_api_resolver_free(subghz->api_resolver);
        subghz->api_resolver = NULL;
    }
}
