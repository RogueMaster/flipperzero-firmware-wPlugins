// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file plugin_host.h
 * Load/unload a FlipDeFlock plugin from the app's own asset directory.
 *
 * The heavy, rarely-reached modules (the QR encoder; the ESP32 flasher) ship as
 * .fal files embedded in the .fap and are mapped into RAM only while the screen
 * that needs them is open. That is what keeps them out of the contiguous
 * allocation the loader must find at launch -- the failure users were hitting
 * (issue #5).
 *
 * FAIL-SAFE BY CONTRACT: every function here returns NULL/void on any problem
 * -- missing asset directory, version mismatch, corrupt .fal -- and never
 * traps. A caller that gets NULL must show "unavailable" and let the user back
 * out. The asset directory can legitimately be missing (a user who copied the
 * .fap onto a card the firmware has not extracted assets for yet), so treating
 * that as fatal would turn a cosmetic problem into a dead app.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PluginHost PluginHost;

/**
 * Load the single plugin matching `app_id`/`api_version` from the app's assets.
 *
 * @param app_id       plugin appid, e.g. QR_PLUGIN_APP_ID
 * @param api_version  ABI version the caller was compiled against
 * @param out_api      receives the plugin's API struct pointer on success
 * @return             handle to free with plugin_host_free(), or NULL on any
 *                     failure (in which case *out_api is left NULL)
 */
PluginHost* plugin_host_load(const char* app_id, uint32_t api_version, const void** out_api);

/** Unload and free. NULL-safe. Invalidates the API pointer from load(). */
void plugin_host_free(PluginHost* host);

#ifdef __cplusplus
}
#endif
