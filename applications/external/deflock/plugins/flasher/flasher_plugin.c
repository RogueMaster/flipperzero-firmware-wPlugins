// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// ESP32 flasher plugin. Thin adapter over esp_flasher.c -- the point of the file
// is that esp_flasher.o and the whole vendored esp-serial-flasher live HERE and
// not in the app image. See plugins/flasher_plugin_api.h for the measurements.
#include "../flasher_plugin_api.h"
#include "esp_flasher.h"

#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>

// No adapter shims: the ABI's function-pointer types were written to match
// esp_flasher.h exactly, so these are direct assignments and the compiler checks
// them. `EspFlasher` is the same `struct EspFlasher` on both sides -- opaque to
// the app, defined in esp_flasher.c -- so nothing crossing the boundary is cast.
// If a signature ever drifts, this file fails to compile, which is the point of
// routing every call through one struct instead of exporting six symbols.
static const FlasherPluginApi flasher_plugin_api = {
    .alloc = esp_flasher_alloc,
    .free = esp_flasher_free,
    .connect = esp_flasher_connect,
    .flash_file = esp_flasher_flash_file,
    .backup = esp_flasher_backup,
    .abort = esp_flasher_abort,
};

static const FlipperAppPluginDescriptor flasher_plugin_descriptor = {
    .appid = FLASHER_PLUGIN_APP_ID,
    .ep_api_version = FLASHER_PLUGIN_API_VERSION,
    .entry_point = &flasher_plugin_api,
};

const FlipperAppPluginDescriptor* flasher_plugin_ep(void) {
    return &flasher_plugin_descriptor;
}
