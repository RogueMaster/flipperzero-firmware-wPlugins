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

    // Load THE ONE FILE, not the directory.
    //
    // plugin_manager_load_all() cannot be used here once there is more than one
    // .fal, and the failure is silent. It iterates the directory and BREAKS on
    // the first plugin whose appid does not match the filter -- then returns
    // PluginManagerErrorNone anyway. So whichever .fal the directory happens to
    // yield first wins, and every other plugin becomes unloadable.
    //
    // Concretely: `flipdeflock_flasher.fal` sorts before `flipdeflock_qr.fal`,
    // so asking for the QR encoder scanned the flasher, mismatched, broke out,
    // and reported "no plugin" with a perfectly healthy .fal sitting in the same
    // folder. The QR handoff would have died the moment the flasher shipped.
    //
    // A plugin's asset is named "<appid>.fal", so the path is derivable and the
    // scan buys nothing. load_single is also deterministic and does not depend
    // on readdir order.
    char path[96];
    int n = snprintf(path, sizeof(path), "%s/%s.fal", PLUGIN_DIR, app_id);
    if(n < 0 || (size_t)n >= sizeof(path)) {
        FURI_LOG_E(TAG, "%s: plugin path too long", app_id);
        plugin_manager_free(manager);
        return NULL;
    }

    PluginManagerError err = plugin_manager_load_single(manager, path);
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
