// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "plugin_host.h"

#include <furi.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#include <stdlib.h>

#define TAG "PluginHost"

/** Where the firmware extracts this app's embedded file assets. Resolved by the
 *  storage service per running app, so it needs no appid baked in here. */
#define PLUGIN_DIR APP_ASSETS_PATH("plugins")

struct PluginHost {
    PluginManager* manager;
};

PluginHost* plugin_host_load(const char* app_id, uint32_t api_version, const void** out_api) {
    furi_check(out_api);
    *out_api = NULL;
    if(!app_id) return NULL;

    PluginManager* manager = plugin_manager_alloc(app_id, api_version, firmware_api_interface);
    if(!manager) {
        FURI_LOG_E(TAG, "%s: manager alloc failed", app_id);
        return NULL;
    }

    // load_all filters by the appid/api_version handed to alloc(), so a .fal
    // for a different plugin -- or a stale one from an older app version -- is
    // skipped rather than mistaken for this one.
    PluginManagerError err = plugin_manager_load_all(manager, PLUGIN_DIR);
    if(err != PluginManagerErrorNone || plugin_manager_get_count(manager) == 0) {
        // Not an error worth shouting about: the usual cause is a card whose
        // assets have not been extracted yet. The caller shows "unavailable".
        FURI_LOG_W(TAG, "%s: no plugin (err=%d)", app_id, (int)err);
        plugin_manager_free(manager);
        return NULL;
    }

    const void* api = plugin_manager_get_ep(manager, 0);
    if(!api) {
        FURI_LOG_E(TAG, "%s: null entry point", app_id);
        plugin_manager_free(manager);
        return NULL;
    }

    PluginHost* host = malloc(sizeof(PluginHost));
    host->manager = manager;
    *out_api = api;
    return host;
}

void plugin_host_free(PluginHost* host) {
    if(!host) return;
    // Frees the manager, which unmaps the plugin's image -- the entire point of
    // loading it on demand. Any API pointer handed out by load() dies here.
    plugin_manager_free(host->manager);
    free(host);
}
