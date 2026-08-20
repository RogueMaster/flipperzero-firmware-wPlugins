// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "plugin_host.h"

#include <furi.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#include <stdbool.h>
#include <stdlib.h>

#define TAG "PluginHost"

/**
 * Where the firmware extracts this app's embedded file assets.
 *
 * "/assets" is a VIRTUAL path. storage_processing.c rewrites it to
 * /ext/apps_assets/<appid>, and for an external .fap that appid is NOT the
 * manifest appid -- the loader takes it from the FILENAME:
 *
 *     path_extract_filename_no_ext(path, app_name);
 *     furi_thread_set_appid(loader->app.thread, ...);
 *
 * Extraction derives its directory the same way, so on stock firmware the two
 * agree and one path is enough. That stops being true the moment the same app
 * ships under more than one filename -- which is exactly what RogueMaster
 * builds do as `deflock.fap` (#21). Anything that makes the running name and
 * the extracted name disagree leaves a perfectly good .fal on the card that the
 * app cannot see, and the only symptom is "unavailable".
 *
 * So the virtual path is tried first (correct whenever the firmware behaves as
 * documented), then the concrete directory for every filename this project
 * actually ships under. Bounded, cheap, and it removes a whole class of
 * "works on my firmware" report.
 */
#define PLUGIN_SUBDIR "plugins"

static const char* const PLUGIN_ASSET_DIRS[] = {
    APP_ASSETS_PATH(PLUGIN_SUBDIR), // per-app virtual path; correct on stock firmware
    // Concrete fallbacks, one per released artifact name. Keep in step with
    // .github/actions/name-fap/action.yml, which decides those filenames.
    "/ext/apps_assets/flipdeflock/" PLUGIN_SUBDIR,
    "/ext/apps_assets/deflock/" PLUGIN_SUBDIR, // RogueMaster
    "/ext/apps_assets/flipdeflock-unleashed/" PLUGIN_SUBDIR,
    "/ext/apps_assets/flipdeflock-momentum/" PLUGIN_SUBDIR,
};

#define PLUGIN_ASSET_DIR_COUNT (sizeof(PLUGIN_ASSET_DIRS) / sizeof(PLUGIN_ASSET_DIRS[0]))

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
    // Try each candidate in turn, and LOG EVERY ATTEMPT with its full path. The
    // previous version logged only "no plugin", which is what turned issue #23
    // into an investigation rather than a one-line answer.
    char path[128];
    bool loaded = false;
    for(size_t i = 0; i < PLUGIN_ASSET_DIR_COUNT && !loaded; i++) {
        int n = snprintf(path, sizeof(path), "%s/%s.fal", PLUGIN_ASSET_DIRS[i], app_id);
        if(n < 0 || (size_t)n >= sizeof(path)) {
            FURI_LOG_E(TAG, "%s: path too long under %s", app_id, PLUGIN_ASSET_DIRS[i]);
            continue;
        }
        PluginManagerError err = plugin_manager_load_single(manager, path);
        if(err == PluginManagerErrorNone && plugin_manager_get_count(manager) > 0) {
            FURI_LOG_I(TAG, "%s: loaded from %s", app_id, path);
            loaded = true;
        } else {
            FURI_LOG_W(TAG, "%s: not at %s (err=%d)", app_id, path, (int)err);
        }
    }

    if(!loaded) {
        // Not worth trapping over: a card whose assets were never extracted is a
        // legitimate state. The caller shows "unavailable"; the log above now
        // names every path that was tried.
        FURI_LOG_E(
            TAG, "%s: no plugin in any of %u asset dirs", app_id, (unsigned)PLUGIN_ASSET_DIR_COUNT);
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
