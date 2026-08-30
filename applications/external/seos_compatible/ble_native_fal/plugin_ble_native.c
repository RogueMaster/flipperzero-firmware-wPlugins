/* The Flipper's own radio, as a loadable plugin.
 *
 * Wraps the native peripheral role so the GATT service and profile stay out
 * of the app's image until a scene asks for them.
 */
#include "../seos_ble_plugin.h"

#include "seos_native_peripheral.h"

#include <flipper_application/flipper_application.h>

static void* ble_native_alloc(Seos* seos) {
    return seos_native_peripheral_alloc(seos);
}

static void ble_native_start(void* ctx, FlowMode mode) {
    seos_native_peripheral_start(ctx, mode);
}

static void ble_native_stop(void* ctx) {
    seos_native_peripheral_stop(ctx);
}

static void ble_native_free(void* ctx) {
    seos_native_peripheral_free(ctx);
}

static const SeosBlePlugin plugin_ble_native = {
    .name = "Native BLE",
    .stack = SeosBleStackNative,
    .central = false,
    .alloc = &ble_native_alloc,
    .start = &ble_native_start,
    .stop = &ble_native_stop,
    .free = &ble_native_free,
};

static const FlipperAppPluginDescriptor plugin_ble_native_descriptor = {
    .appid = SEOS_BLE_PLUGIN_APP_ID,
    .ep_api_version = SEOS_BLE_PLUGIN_API_VERSION,
    .entry_point = &plugin_ble_native,
};

const FlipperAppPluginDescriptor* plugin_ble_native_ep(void) {
    return &plugin_ble_native_descriptor;
}
