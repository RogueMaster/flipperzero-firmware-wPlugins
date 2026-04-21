#include "flippass_output_transport.h"

#include "../plugins/flippass_output_plugin.h"

#include <stdio.h>

static FlipPassModuleSlot flippass_output_transport_slot(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FlipPassModuleSlotOutputBle :
                                                             FlipPassModuleSlotOutputUsb;
}

static const char* flippass_output_transport_appid(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FLIPPASS_OUTPUT_BLE_PLUGIN_APP_ID :
                                                             FLIPPASS_OUTPUT_USB_PLUGIN_APP_ID;
}

static FlipPassOutputPluginTransport
    flippass_output_transport_plugin_transport(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ?
               FlipPassOutputPluginTransportBluetooth :
               FlipPassOutputPluginTransportUsb;
}

static FlipPassOutputTransport flippass_output_transport_other(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FlipPassOutputTransportUsb :
                                                             FlipPassOutputTransportBluetooth;
}

static void flippass_output_plugin_progress(
    void* host_context,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    App* app = host_context;
    if(app == NULL) {
        return;
    }

    flippass_progress_update(
        app, (stage != NULL) ? stage : "Typing", (detail != NULL) ? detail : "", percent);
}

static void
    flippass_output_plugin_log(void* host_context, const char* module_name, const char* message) {
    App* app = host_context;
    if(app == NULL || module_name == NULL || message == NULL) {
        return;
    }

    FLIPPASS_LOG_EVENT(app, "%s %s", module_name, message);
}

static bool flippass_output_plugin_should_cancel(void* host_context) {
    return flippass_typing_should_cancel(host_context);
}

static FlipPassOutputPluginHostApiV1 flippass_output_transport_host_api(App* app) {
    const FlipPassOutputPluginHostApiV1 host_api = {
        .api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
        .host_context = app,
        .progress = flippass_output_plugin_progress,
        .log = flippass_output_plugin_log,
        .should_cancel = flippass_output_plugin_should_cancel,
    };
    return host_api;
}

static const FlipPassOutputPluginV1*
    flippass_output_transport_plugin_loaded(const App* app, FlipPassOutputTransport transport) {
    if(app == NULL) {
        return NULL;
    }

    const FlipPassModuleInstance* instance =
        &app->module_loader.slot[flippass_output_transport_slot(transport)];
    return (instance->descriptor != NULL) ? instance->descriptor->entry_point : NULL;
}

static const FlipPassOutputPluginV1*
    flippass_output_transport_plugin_ensure(App* app, FlipPassOutputTransport transport) {
    FuriString* error = furi_string_alloc();
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        flippass_output_transport_slot(transport),
        NULL,
        flippass_output_transport_appid(transport),
        FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
        error);

    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(app != NULL) {
            FLIPPASS_LOG_EVENT(
                app,
                "OUTPUT_PLUGIN_LOAD_FAIL transport=%s reason=%s",
                flippass_output_transport_name(transport),
                furi_string_get_cstr(error));
        }
        furi_string_free(error);
        return NULL;
    }

    const FlipPassOutputPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_OUTPUT_PLUGIN_API_VERSION ||
       plugin->transport != flippass_output_transport_plugin_transport(transport)) {
        if(app != NULL) {
            FLIPPASS_LOG_EVENT(
                app,
                "OUTPUT_PLUGIN_API_FAIL transport=%s",
                flippass_output_transport_name(transport));
        }
        flippass_module_unload(app, flippass_output_transport_slot(transport));
        furi_string_free(error);
        return NULL;
    }

    furi_string_free(error);
    return plugin;
}

static void flippass_output_transport_cleanup_loaded(App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin != NULL && plugin->cleanup != NULL) {
        const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
        plugin->cleanup(&host_api);
    }

    if(transport == FlipPassOutputTransportUsb) {
        app->usb_expect_rpc_session_close = false;
    }

    flippass_module_unload(app, flippass_output_transport_slot(transport));
}

static void flippass_output_transport_cleanup_other(App* app, FlipPassOutputTransport transport) {
    if(app == NULL) {
        return;
    }

    flippass_output_transport_cleanup_loaded(app, flippass_output_transport_other(transport));
}

static void flippass_output_transport_trim_idle(App* app, FlipPassOutputTransport transport) {
    if(app == NULL || transport != FlipPassOutputTransportBluetooth) {
        return;
    }

    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL) {
        return;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    const bool connected = (plugin->is_connected != NULL) && plugin->is_connected(&host_api);
    const bool advertising = (plugin->is_advertising != NULL) && plugin->is_advertising(&host_api);

    if(!connected && !advertising) {
        flippass_output_transport_cleanup_loaded(app, transport);
    }
}

static void flippass_output_transport_finish_loaded(App* app, FlipPassOutputTransport transport) {
    flippass_output_transport_cleanup_loaded(app, transport);
}

bool flippass_output_transport_begin(App* app, FlipPassOutputTransport transport) {
    flippass_output_transport_cleanup_other(app, transport);

    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_ensure(app, transport);
    if(plugin == NULL || plugin->begin == NULL) {
        return false;
    }

    if(transport == FlipPassOutputTransportUsb) {
        app->usb_expect_rpc_session_close = app->rpc_mode;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    const bool ok = plugin->begin(&host_api);
    if(!ok) {
        flippass_output_transport_finish_loaded(app, transport);
    }

    return ok;
}

void flippass_output_transport_end(App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL) {
        return;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    if(plugin->end != NULL) {
        plugin->end(&host_api);
    }

    if(transport == FlipPassOutputTransportUsb) {
        app->usb_expect_rpc_session_close = false;
    }

    flippass_output_transport_finish_loaded(app, transport);
}

bool flippass_output_transport_press_prepared(
    App* app,
    FlipPassOutputTransport transport,
    uint16_t hid_key) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL || plugin->press_key == NULL) {
        return false;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    return plugin->press_key(&host_api, hid_key);
}

bool flippass_output_transport_release_prepared(
    App* app,
    FlipPassOutputTransport transport,
    uint16_t hid_key) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL || plugin->release_key == NULL) {
        return false;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    return plugin->release_key(&host_api, hid_key);
}

void flippass_output_transport_release_all_prepared(App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL || plugin->release_all == NULL) {
        return;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    plugin->release_all(&host_api);
}

bool flippass_output_transport_is_connected(const App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL || plugin->is_connected == NULL) {
        return false;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api((App*)app);
    const bool connected = plugin->is_connected(&host_api);
    if(transport == FlipPassOutputTransportBluetooth) {
        const bool advertising = (plugin->is_advertising != NULL) &&
                                 plugin->is_advertising(&host_api);
        if(!connected && !advertising) {
            flippass_output_transport_trim_idle((App*)app, transport);
        }
    }

    return connected;
}

bool flippass_output_transport_is_advertising(const App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin == NULL || plugin->is_advertising == NULL) {
        return false;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api((App*)app);
    const bool advertising = plugin->is_advertising(&host_api);
    if(transport == FlipPassOutputTransportBluetooth) {
        const bool connected = (plugin->is_connected != NULL) && plugin->is_connected(&host_api);
        if(!connected && !advertising) {
            flippass_output_transport_trim_idle((App*)app, transport);
        }
    }

    return advertising;
}

bool flippass_output_transport_advertise(App* app, FlipPassOutputTransport transport) {
    flippass_output_transport_cleanup_other(app, transport);

    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_ensure(app, transport);
    if(plugin == NULL || plugin->advertise == NULL) {
        return false;
    }

    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    const bool ok = plugin->advertise(&host_api);
    if(!ok) {
        flippass_output_transport_finish_loaded(app, transport);
    }

    return ok;
}

void flippass_output_transport_get_name(
    App* app,
    FlipPassOutputTransport transport,
    char* buffer,
    size_t size) {
    if(buffer == NULL || size == 0U) {
        return;
    }

    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin != NULL && plugin->get_name != NULL) {
        plugin->get_name(buffer, size);
    } else {
        switch(transport) {
        case FlipPassOutputTransportBluetooth:
            snprintf(buffer, size, "BadUSB %s", furi_hal_version_get_name_ptr());
            break;
        case FlipPassOutputTransportUsb:
        default:
            snprintf(buffer, size, "USB HID");
            break;
        }
    }
}

void flippass_output_transport_cleanup(App* app, FlipPassOutputTransport transport) {
    flippass_output_transport_cleanup_loaded(app, transport);
}

bool flippass_usb_begin(App* app) {
    return flippass_output_transport_begin(app, FlipPassOutputTransportUsb);
}

void flippass_usb_restore(App* app) {
    flippass_output_transport_cleanup(app, FlipPassOutputTransportUsb);
}

static bool flippass_usb_send_key(App* app, uint16_t hid_key) {
    if(hid_key == HID_KEYBOARD_NONE) {
        return false;
    }

    if(!flippass_output_transport_press_prepared(app, FlipPassOutputTransportUsb, hid_key)) {
        flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_PRESS_DELAY_MS);
    if(!flippass_output_transport_release_prepared(app, FlipPassOutputTransportUsb, hid_key)) {
        flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    return true;
}

static bool flippass_usb_send_special_key_prepared(App* app, uint16_t hid_key) {
    if(!flippass_usb_send_key(app, hid_key)) {
        return false;
    }

    furi_delay_ms(FLIPPASS_USB_STEP_DELAY_MS);
    return true;
}

static bool flippass_usb_type_string_prepared(App* app, const char* text) {
    furi_assert(app);
    furi_assert(text);

    for(size_t i = 0U; text[i] != '\0'; i++) {
        if(!flippass_usb_send_key(app, HID_ASCII_TO_KEY(text[i]))) {
            return false;
        }
        furi_delay_ms(FLIPPASS_USB_STEP_DELAY_MS);
    }

    return true;
}

bool flippass_usb_type_string(App* app, const char* text) {
    furi_assert(app);
    furi_assert(text);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    const bool ok = flippass_usb_type_string_prepared(app, text);
    flippass_usb_restore(app);
    return ok;
}

bool flippass_usb_type_login(App* app, const char* username, const char* password) {
    furi_assert(app);
    furi_assert(username);
    furi_assert(password);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_type_string_prepared(app, username)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_send_special_key_prepared(app, HID_KEYBOARD_TAB)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_type_string_prepared(app, password)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_send_special_key_prepared(app, HID_KEYBOARD_RETURN)) {
        flippass_usb_restore(app);
        return false;
    }

    flippass_usb_restore(app);
    return true;
}

bool flippass_usb_type_key(App* app, uint16_t hid_key) {
    furi_assert(app);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    const bool ok = flippass_usb_send_key(app, hid_key);
    flippass_usb_restore(app);
    return ok;
}

bool flippass_usb_type_autotype(App* app, const KDBXEntry* entry) {
    furi_assert(app);
    furi_assert(entry);

    return flippass_output_type_autotype(app, FlipPassOutputTransportUsb, entry);
}

bool flippass_output_bluetooth_is_connected(const App* app) {
    return flippass_output_transport_is_connected(app, FlipPassOutputTransportBluetooth);
}

bool flippass_output_bluetooth_is_advertising(const App* app) {
    return flippass_output_transport_is_advertising(app, FlipPassOutputTransportBluetooth);
}

bool flippass_output_bluetooth_advertise(App* app) {
    return flippass_output_transport_advertise(app, FlipPassOutputTransportBluetooth);
}

void flippass_output_bluetooth_get_name(char* buffer, size_t size) {
    if(buffer == NULL || size == 0U) {
        return;
    }

    snprintf(buffer, size, "BadUSB %s", furi_hal_version_get_name_ptr());
}

void flippass_output_release_all(App* app) {
    furi_assert(app);

    flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
    flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportBluetooth);
}

void flippass_output_cleanup(App* app) {
    furi_assert(app);

    flippass_output_transport_cleanup(app, FlipPassOutputTransportUsb);
    flippass_output_transport_cleanup(app, FlipPassOutputTransportBluetooth);
}
