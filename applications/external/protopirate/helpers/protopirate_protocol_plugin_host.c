#include "../protopirate_app_i.h"
#include "protopirate_txrx.h"
#include "../protocols/protocol_items.h"

#include <loader/firmware_api/firmware_api.h>
#include <stdio.h>
#include <string.h>

#define TAG "ProtoPirateProtocolPlugin"
#ifdef ENABLE_EMULATE_FEATURE
#define PROTOPIRATE_TX_PLUGIN_PATH_MAX 160U
#endif

static const char* protopirate_get_registry_plugin_path(ProtoPirateProtocolRegistryRoute route) {
    switch(route) {
    case ProtoPirateProtocolRegistryRouteAMVag:
        return APP_ASSETS_PATH("plugins/protopirate_am_vag_plugin.fal");
    case ProtoPirateProtocolRegistryRouteFMDefault:
        return APP_ASSETS_PATH("plugins/protopirate_fm_plugin.fal");
    case ProtoPirateProtocolRegistryRouteFMF4:
        return APP_ASSETS_PATH("plugins/protopirate_fm_f4_plugin.fal");
    case ProtoPirateProtocolRegistryRouteFMHonda1:
        return APP_ASSETS_PATH("plugins/protopirate_fm_honda1_plugin.fal");
    case ProtoPirateProtocolRegistryRouteAMDefault:
    default:
        return APP_ASSETS_PATH("plugins/protopirate_am_plugin.fal");
    }
}

#ifdef ENABLE_EMULATE_FEATURE
static bool protopirate_build_tx_protocol_plugin_path(
    const char* tx_key,
    char* plugin_path,
    size_t plugin_path_size) {
    if(!tx_key || !plugin_path || plugin_path_size == 0U) {
        return false;
    }

    int written = snprintf(
        plugin_path,
        plugin_path_size,
        APP_ASSETS_PATH("plugins/protopirate_tx_%s_plugin.fal"),
        tx_key);
    return (written > 0) && ((size_t)written < plugin_path_size);
}
#endif

static const SubGhzProtocolRegistry protopirate_empty_protocol_registry = {
    .items = NULL,
    .size = 0,
};

void protopirate_unload_protocol_plugin(ProtoPirateTxRx* txrx) {
    furi_check(txrx);

    if(txrx->environment) {
        subghz_environment_set_protocol_registry(
            txrx->environment, &protopirate_empty_protocol_registry);
    }

    txrx->protocol_registry = NULL;

    if(txrx->protocol_plugin && txrx->protocol_plugin->release) {
        txrx->protocol_plugin->release();
    }
    txrx->protocol_plugin = NULL;

    if(txrx->protocol_plugin_manager) {
        plugin_manager_free(txrx->protocol_plugin_manager);
        txrx->protocol_plugin_manager = NULL;
    }

    if(txrx->plugin_resolver) {
        composite_api_resolver_free(txrx->plugin_resolver);
        txrx->plugin_resolver = NULL;
    }
}

static bool protopirate_ensure_protocol_registry_plugin(
    ProtoPirateApp* app,
    ProtoPirateProtocolRegistryRoute route,
    const SubGhzProtocolRegistry** registry) {
    furi_check(app);
    furi_check(app->txrx);
    furi_check(registry);

    *registry = NULL;

    if(!app->txrx->environment) {
        FURI_LOG_E(TAG, "Cannot load protocol plugin without radio environment");
        return false;
    }

    if(app->txrx->protocol_plugin &&
       app->txrx->protocol_plugin->kind == ProtoPirateProtocolPluginKindRx &&
       app->txrx->protocol_plugin->registry && app->txrx->protocol_registry_route == route) {
        *registry = app->txrx->protocol_plugin->registry;
        return true;
    }

    if(app->txrx->protocol_plugin || app->txrx->protocol_plugin_manager ||
       app->txrx->plugin_resolver) {
        protopirate_unload_protocol_plugin(app->txrx);
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate protocol plugin resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
        PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate protocol plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    const char* plugin_path = protopirate_get_registry_plugin_path(route);
    PluginManagerError error = plugin_manager_load_single(manager, plugin_path);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load protocol plugin %s: %d", plugin_path, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const ProtoPirateProtocolPlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || !plugin->registry) {
        FURI_LOG_E(TAG, "Protocol plugin entry point is invalid");
        if(plugin && plugin->release) {
            plugin->release();
        }
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    if(plugin->kind != ProtoPirateProtocolPluginKindRx) {
        FURI_LOG_E(TAG, "Protocol plugin kind mismatch for RX route");
        if(plugin->release) {
            plugin->release();
        }
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    if(plugin->route != route) {
        FURI_LOG_E(
            TAG, "Protocol plugin route mismatch (expected %d got %d)", route, plugin->route);
        if(plugin->release) {
            plugin->release();
        }
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->txrx->plugin_resolver = resolver;
    app->txrx->protocol_plugin_manager = manager;
    app->txrx->protocol_plugin = plugin;
    app->txrx->protocol_registry_route = route;
    *registry = plugin->registry;
    return true;
}

#ifdef ENABLE_EMULATE_FEATURE
static bool protopirate_ensure_tx_protocol_plugin(
    ProtoPirateApp* app,
    const char* protocol_name,
    const SubGhzProtocolRegistry** registry) {
    furi_check(app);
    furi_check(app->txrx);
    furi_check(registry);

    *registry = NULL;

    if(!app->txrx->environment) {
        FURI_LOG_E(TAG, "Cannot load TX protocol plugin without radio environment");
        return false;
    }

    const ProtoPirateProtocolCatalogEntry* catalog_entry =
        protopirate_protocol_catalog_find(protocol_name);
    const char* registry_name = protopirate_protocol_catalog_canonical_name(protocol_name);
    const char* tx_key = protopirate_protocol_catalog_tx_key(protocol_name);
    char plugin_path[PROTOPIRATE_TX_PLUGIN_PATH_MAX];
    if(catalog_entry && !tx_key) {
        FURI_LOG_W(TAG, "TX disabled for %s: protocol catalog has no tx_key", registry_name);
        return false;
    }
    if(!registry_name || !tx_key ||
       !protopirate_build_tx_protocol_plugin_path(tx_key, plugin_path, sizeof(plugin_path))) {
        FURI_LOG_E(TAG, "No TX protocol plugin for %s", protocol_name ? protocol_name : "?");
        return false;
    }

    if(app->txrx->protocol_plugin &&
       app->txrx->protocol_plugin->kind == ProtoPirateProtocolPluginKindTx &&
       app->txrx->protocol_plugin->registry && app->txrx->protocol_plugin->protocol_name &&
       strcmp(app->txrx->protocol_plugin->protocol_name, registry_name) == 0) {
        *registry = app->txrx->protocol_plugin->registry;
        return true;
    }

    if(app->txrx->protocol_plugin || app->txrx->protocol_plugin_manager ||
       app->txrx->plugin_resolver) {
        protopirate_unload_protocol_plugin(app->txrx);
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate TX protocol plugin resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID,
        PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate TX protocol plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    PluginManagerError error = plugin_manager_load_single(manager, plugin_path);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load TX protocol plugin %s: %d", plugin_path, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const ProtoPirateProtocolPlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || plugin->kind != ProtoPirateProtocolPluginKindTx || !plugin->registry ||
       plugin->registry->size == 0U || !plugin->protocol_name ||
       strcmp(plugin->protocol_name, registry_name) != 0) {
        FURI_LOG_E(TAG, "TX protocol plugin entry point is invalid for %s", registry_name);
        if(plugin && plugin->release) {
            plugin->release();
        }
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const SubGhzProtocol* tx_protocol = plugin->registry->items[0];
    if(!tx_protocol || !tx_protocol->encoder || !tx_protocol->encoder->alloc ||
       !tx_protocol->encoder->deserialize || !tx_protocol->encoder->yield) {
        FURI_LOG_E(TAG, "TX protocol plugin for %s has no encoder", registry_name);
        if(plugin->release) {
            plugin->release();
        }
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->txrx->plugin_resolver = resolver;
    app->txrx->protocol_plugin_manager = manager;
    app->txrx->protocol_plugin = plugin;
    *registry = plugin->registry;
    return true;
}
#endif

bool protopirate_refresh_protocol_registry(ProtoPirateApp* app, bool ensure_receiver_ready) {
    furi_check(app);
    furi_check(app->txrx);

    if(!app->txrx->environment || !app->txrx->preset) {
        return true;
    }

    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    ProtoPirateProtocolRegistryRoute route = protopirate_get_protocol_registry_route(
        preset_name,
        app->txrx->preset->frequency,
        app->txrx->preset->data,
        app->txrx->preset->data_size,
        NULL);
    bool route_changed = !app->txrx->protocol_plugin ||
                         (app->txrx->protocol_plugin->kind != ProtoPirateProtocolPluginKindRx) ||
                         (app->txrx->protocol_registry_route != route);

    if(route_changed) {
        protopirate_rx_stack_teardown_for_registry_switch(app);
    } else if(ensure_receiver_ready && !app->txrx->receiver) {
        protopirate_rx_stack_teardown_for_registry_switch(app);
    }

    const SubGhzProtocolRegistry* registry = NULL;
    if(!protopirate_ensure_protocol_registry_plugin(app, route, &registry) || !registry) {
        FURI_LOG_E(
            TAG,
            "Failed to resolve %s protocol registry plugin",
            protopirate_get_protocol_registry_route_name(route));
        return false;
    }

    const bool registry_already_bound = (app->txrx->protocol_registry == registry);
    if(!registry_already_bound) {
        FURI_LOG_I(
            TAG,
            "Using %s protocol registry (%zu protocols)",
            protopirate_get_protocol_registry_route_name(route),
            registry->size);
        subghz_environment_set_protocol_registry(app->txrx->environment, registry);
        app->txrx->protocol_registry = registry;
    }

    if(!ensure_receiver_ready) {
        return true;
    }

    if(app->txrx->receiver) {
        return true;
    }

    app->txrx->receiver = subghz_receiver_alloc_init(app->txrx->environment);
    if(!app->txrx->receiver) {
        FURI_LOG_E(
            TAG,
            "Failed to allocate receiver for %s registry",
            protopirate_get_protocol_registry_route_name(route));
        return false;
    }

    subghz_receiver_set_filter(app->txrx->receiver, SubGhzProtocolFlag_Decodable);
    return true;
}

static bool protopirate_ensure_receiver_allocated(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    if(app->txrx->receiver) {
        return true;
    }
    if(!app->txrx->environment) {
        return false;
    }

    app->txrx->receiver = subghz_receiver_alloc_init(app->txrx->environment);
    if(!app->txrx->receiver) {
        FURI_LOG_E(TAG, "Failed to allocate receiver after registry restore");
        return false;
    }

    subghz_receiver_set_filter(app->txrx->receiver, SubGhzProtocolFlag_Decodable);
    return true;
}

bool protopirate_apply_protocol_registry_for_context(
    ProtoPirateApp* app,
    const char* preset_name,
    uint32_t frequency,
    const uint8_t* preset_data,
    size_t preset_data_size,
    const char* protocol_name) {
    furi_check(app);
    furi_check(app->txrx);

    if(!app->txrx->environment) {
        return false;
    }

    if(!preset_name && app->txrx->preset && app->txrx->preset->name) {
        preset_name = furi_string_get_cstr(app->txrx->preset->name);
    }
    if(frequency == 0U && app->txrx->preset) {
        frequency = app->txrx->preset->frequency;
    }
    if((!preset_data || preset_data_size == 0U) && app->txrx->preset) {
        preset_data = app->txrx->preset->data;
        preset_data_size = app->txrx->preset->data_size;
    }

    if(protocol_name && protocol_name[0] != '\0') {
#ifdef ENABLE_EMULATE_FEATURE
        const char* registry_name = protopirate_protocol_catalog_canonical_name(protocol_name);
        if(!registry_name || !protopirate_protocol_catalog_can_tx(protocol_name)) {
            FURI_LOG_E(TAG, "No TX protocol plugin for %s", protocol_name);
            return false;
        }
        bool tx_changed = !app->txrx->protocol_plugin ||
                          (app->txrx->protocol_plugin->kind != ProtoPirateProtocolPluginKindTx) ||
                          !app->txrx->protocol_plugin->protocol_name ||
                          strcmp(app->txrx->protocol_plugin->protocol_name, registry_name) != 0;

        if(tx_changed) {
            protopirate_rx_stack_teardown_for_registry_switch(app);
        }

        const SubGhzProtocolRegistry* tx_registry = NULL;
        if(!protopirate_ensure_tx_protocol_plugin(app, registry_name, &tx_registry) ||
           !tx_registry) {
            FURI_LOG_E(TAG, "Failed to resolve TX protocol plugin for %s", protocol_name);
            return false;
        }

        if(app->txrx->protocol_registry == tx_registry) {
            return true;
        }

        FURI_LOG_I(
            TAG,
            "Switching active protocol registry to TX %s",
            registry_name ? registry_name : "?");
        subghz_environment_set_protocol_registry(app->txrx->environment, tx_registry);
        app->txrx->protocol_registry = tx_registry;
        return true;
#else
        return false;
#endif
    }

    ProtoPirateProtocolRegistryRoute route = protopirate_get_protocol_registry_route(
        preset_name, frequency, preset_data, preset_data_size, NULL);

    bool route_changed = !app->txrx->protocol_plugin ||
                         (app->txrx->protocol_plugin->kind != ProtoPirateProtocolPluginKindRx) ||
                         (app->txrx->protocol_registry_route != route);

    if(route_changed) {
        protopirate_rx_stack_teardown_for_registry_switch(app);
    }

    const SubGhzProtocolRegistry* registry = NULL;
    if(!protopirate_ensure_protocol_registry_plugin(app, route, &registry) || !registry) {
        FURI_LOG_E(
            TAG,
            "Failed to resolve %s registry plugin for preset apply",
            protopirate_get_protocol_registry_route_name(route));
        return false;
    }

    if(app->txrx->protocol_registry == registry) {
        return protopirate_ensure_receiver_allocated(app);
    }

    FURI_LOG_I(
        TAG,
        "Switching active protocol registry to %s (%zu protocols)",
        protopirate_get_protocol_registry_route_name(route),
        registry->size);
    subghz_environment_set_protocol_registry(app->txrx->environment, registry);
    app->txrx->protocol_registry = registry;
    return protopirate_ensure_receiver_allocated(app);
}
