/* The external nRF52840 stack, as a loadable plugin.
 *
 * Nothing here is new: it wraps the existing peripheral and central roles so
 * the whole serial and HCI stack stays out of the app's image until a scene
 * asks for it.
 */
#include "../seos_ble_plugin.h"

#include "seos_characteristic.h"
#include "seos_central.h"

#include <flipper_application/flipper_application.h>

/* Which role a context drives is fixed when it is allocated. The peripheral
 * and central roles never run at once. */
typedef struct {
    SeosCharacteristic* peripheral;
    SeosCentral* central;
    Seos* seos;
} BleExtContext;

static void* ble_ext_alloc(Seos* seos) {
    /* The dongle is powered from the OTG rail, and only while it is in use. */
    furi_hal_power_enable_otg();

    /* Nothing here can work without the serial port. Another app or the
     * expansion service may hold it, in which case say so now rather than
     * asserting deep inside the worker thread. */
    FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if(!handle) {
        FURI_LOG_W("BleExt", "Serial port is not available");
        furi_hal_power_disable_otg();
        return NULL;
    }
    furi_hal_serial_control_release(handle);

    BleExtContext* context = malloc(sizeof(BleExtContext));
    memset(context, 0, sizeof(BleExtContext));
    context->seos = seos;
    return context;
}

static void ble_ext_start(void* ctx, FlowMode mode) {
    BleExtContext* context = ctx;
    if(mode == FLOW_READER_SCANNER || mode == FLOW_CRED_SCANNER) {
        context->central = seos_central_alloc(context->seos);
        seos_central_start(context->central, mode);
    } else {
        context->peripheral = seos_characteristic_alloc(context->seos);
        seos_characteristic_start(context->peripheral, mode);
    }
}

static void ble_ext_stop(void* ctx) {
    BleExtContext* context = ctx;
    if(context->peripheral) seos_characteristic_stop(context->peripheral);
    if(context->central) seos_central_stop(context->central);
}

static void ble_ext_free(void* ctx) {
    BleExtContext* context = ctx;
    if(context->peripheral) seos_characteristic_free(context->peripheral);
    if(context->central) seos_central_free(context->central);
    furi_hal_power_disable_otg();
    free(context);
}

static const SeosBlePlugin plugin_ble_ext = {
    .name = "External BLE",
    .stack = SeosBleStackExternal,
    .central = true,
    .alloc = &ble_ext_alloc,
    .start = &ble_ext_start,
    .stop = &ble_ext_stop,
    .free = &ble_ext_free,
};

static const FlipperAppPluginDescriptor plugin_ble_ext_descriptor = {
    .appid = SEOS_BLE_PLUGIN_APP_ID,
    .ep_api_version = SEOS_BLE_PLUGIN_API_VERSION,
    .entry_point = &plugin_ble_ext,
};

const FlipperAppPluginDescriptor* plugin_ble_ext_ep(void) {
    return &plugin_ble_ext_descriptor;
}
