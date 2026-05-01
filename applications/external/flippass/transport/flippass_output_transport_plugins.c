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

static bool flippass_output_transport_advertise_impl(
    App* app,
    FlipPassOutputTransport transport,
    bool cleanup_other) {
    if(cleanup_other) {
        flippass_output_transport_cleanup_other(app, transport);
    }
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

bool flippass_output_transport_advertise(App* app, FlipPassOutputTransport transport) {
    return flippass_output_transport_advertise_impl(app, transport, true);
}

bool flippass_output_transport_prewarm(App* app, FlipPassOutputTransport transport) {
    return flippass_output_transport_advertise_impl(app, transport, false);
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
