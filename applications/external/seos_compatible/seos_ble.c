#include "seos_ble.h"

#include "seos_i.h"
#include "seos_app_api_interface.h"

#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <flipper_application/plugins/composite_resolver.h>
#include <loader/firmware_api/firmware_api.h>

#define TAG "SeosBle"

#define BLE_EXT_PLUGIN_PATH    APP_ASSETS_PATH("plugins/plugin_ble_ext.fal")
#define BLE_NATIVE_PLUGIN_PATH APP_ASSETS_PATH("plugins/plugin_ble_native.fal")

bool seos_ble_is_loaded(Seos* seos) {
    return seos->ble_plugin != NULL;
}

bool seos_ble_acquire(Seos* seos, SeosBleStack stack) {
    furi_assert(seos);

    if(seos->ble_plugin) {
        return seos->ble_plugin->stack == stack;
    }

    const char* path = stack == SeosBleStackExternal ? BLE_EXT_PLUGIN_PATH :
                                                       BLE_NATIVE_PLUGIN_PATH;

    /* The plugin calls back into the app, so it needs the app's own symbols
     * as well as the firmware's. */
    seos->ble_resolver = composite_api_resolver_alloc();
    composite_api_resolver_add(seos->ble_resolver, firmware_api_interface);
    composite_api_resolver_add(seos->ble_resolver, application_api_interface);

    seos->ble_manager = plugin_manager_alloc(
        SEOS_BLE_PLUGIN_APP_ID,
        SEOS_BLE_PLUGIN_API_VERSION,
        composite_api_resolver_get(seos->ble_resolver));

    FURI_LOG_I(TAG, "Loading %s", path);
    if(plugin_manager_load_single(seos->ble_manager, path) != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Could not load %s", path);
        seos_ble_release(seos);
        return false;
    }

    if(plugin_manager_get_count(seos->ble_manager) == 0) {
        FURI_LOG_E(TAG, "No plugin in %s", path);
        seos_ble_release(seos);
        return false;
    }

    seos->ble_plugin = plugin_manager_get_ep(seos->ble_manager, 0);
    if(!seos->ble_plugin) {
        FURI_LOG_E(TAG, "No entry point in %s", path);
        seos_ble_release(seos);
        return false;
    }

    seos->ble_context = seos->ble_plugin->alloc(seos);
    if(!seos->ble_context) {
        FURI_LOG_E(TAG, "Plugin would not start");
        seos_ble_release(seos);
        return false;
    }

    FURI_LOG_I(TAG, "Loaded %s", seos->ble_plugin->name);
    return true;
}

bool seos_ble_acquire_role(Seos* seos, SeosBleRole role) {
    switch(seos_ble_choose_stack(seos->has_external_ble, role)) {
    case SeosBleChoiceExternal:
        return seos_ble_acquire(seos, SeosBleStackExternal);
    case SeosBleChoiceNative:
        return seos_ble_acquire(seos, SeosBleStackNative);
    case SeosBleChoiceNone:
        FURI_LOG_W(TAG, "No BLE stack can serve this role");
        return false;
    }
    return false;
}

void seos_ble_start(Seos* seos, FlowMode mode) {
    if(!seos->ble_plugin || !seos->ble_context) return;
    seos->ble_plugin->start(seos->ble_context, mode);
}

void seos_ble_release(Seos* seos) {
    furi_assert(seos);

    /* Order matters. The plugin's own threads and callbacks must be stopped
     * and its context freed before the code they run is unmapped. */
    if(seos->ble_plugin && seos->ble_context) {
        seos->ble_plugin->stop(seos->ble_context);
        seos->ble_plugin->free(seos->ble_context);
    }
    seos->ble_context = NULL;
    seos->ble_plugin = NULL;

    if(seos->ble_manager) {
        plugin_manager_free(seos->ble_manager);
        seos->ble_manager = NULL;
    }
    if(seos->ble_resolver) {
        composite_api_resolver_free(seos->ble_resolver);
        seos->ble_resolver = NULL;
    }
}
