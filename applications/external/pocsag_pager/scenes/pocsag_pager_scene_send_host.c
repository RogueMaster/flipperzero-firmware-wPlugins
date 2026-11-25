#include "../pocsag_pager_app_i.h"
#include "../plugins/pocsag_send_plugin.h"

#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#define TAG                  "PCSGSendHost"
#define PCSG_SEND_PLUGIN_FAL APP_ASSETS_PATH("plugins/pocsag_pager_send.fal")

static void pcsg_send_host_save_settings(const PCSGSettings* settings) {
    pcsg_settings_save(settings);
}

static void pcsg_send_host_freq_changed(void* host_context, uint32_t frequency) {
    POCSAGPagerApp* app = host_context;
    app->txrx->preset->frequency = frequency;
}

static void pcsg_send_host_unload(POCSAGPagerApp* app) {
    if(app->send_plugin_ctx && app->send_plugin_api) {
        const PCSGSendPluginInterface* iface = app->send_plugin_api;
        iface->destroy(app->send_plugin_ctx);
    }
    app->send_plugin_ctx = NULL;
    app->send_plugin_api = NULL;

    if(app->send_plugin_app) {
        flipper_application_free(app->send_plugin_app);
        app->send_plugin_app = NULL;
    }
    if(app->send_plugin_storage) {
        furi_record_close(RECORD_STORAGE);
        app->send_plugin_storage = NULL;
    }
}

void pocsag_pager_scene_send_host_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    app->send_plugin_storage = furi_record_open(RECORD_STORAGE);
    app->send_plugin_app =
        flipper_application_alloc(app->send_plugin_storage, firmware_api_interface);

    bool loaded = false;
    do {
        FlipperApplicationPreloadStatus preload =
            flipper_application_preload(app->send_plugin_app, PCSG_SEND_PLUGIN_FAL);
        if(preload != FlipperApplicationPreloadStatusSuccess) {
            FURI_LOG_E(TAG, "Preload failed (%d): %s", preload, PCSG_SEND_PLUGIN_FAL);
            break;
        }
        if(!flipper_application_is_plugin(app->send_plugin_app)) {
            FURI_LOG_E(TAG, "Not a plugin");
            break;
        }
        FlipperApplicationLoadStatus map_res =
            flipper_application_map_to_memory(app->send_plugin_app);
        if(map_res != FlipperApplicationLoadStatusSuccess) {
            FURI_LOG_E(TAG, "Map to memory failed (%d)", map_res);
            break;
        }

        const FlipperAppPluginDescriptor* desc =
            flipper_application_plugin_get_descriptor(app->send_plugin_app);
        if(!desc || strcmp(desc->appid, PCSG_SEND_PLUGIN_APP_ID) != 0 ||
           desc->ep_api_version != PCSG_SEND_PLUGIN_API_VERSION) {
            FURI_LOG_E(TAG, "Plugin descriptor mismatch");
            break;
        }

        const PCSGSendPluginInterface* iface = desc->entry_point;
        if(!iface || !iface->create || !iface->on_event || !iface->destroy) {
            FURI_LOG_E(TAG, "Invalid plugin interface");
            break;
        }

        PCSGSendHostApi host_api = {
            .view_dispatcher = app->view_dispatcher,
            .notifications = app->notifications,
            .setting = app->setting,
            .radio_device = app->txrx->radio_device,
            .is_external_radio = radio_device_loader_is_external(app->txrx->radio_device),
            .settings = &app->send_settings,
            .view_id_base = POCSAGPagerViewSendPluginBase,
            .save_settings = pcsg_send_host_save_settings,
            .on_frequency_changed = pcsg_send_host_freq_changed,
            .host_context = app,
        };

        app->send_plugin_api = iface;
        app->send_plugin_ctx = iface->create(&host_api);
        loaded = (app->send_plugin_ctx != NULL);
    } while(false);

    if(!loaded) {
        pcsg_send_host_unload(app);
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Failed to load\nSend plugin (.fal)\nCheck free RAM / SD");
        view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
    }
}

bool pocsag_pager_scene_send_host_on_event(void* context, SceneManagerEvent event) {
    POCSAGPagerApp* app = context;
    if(app->send_plugin_ctx && app->send_plugin_api) {
        const PCSGSendPluginInterface* iface = app->send_plugin_api;
        return iface->on_event(app->send_plugin_ctx, event);
    }
    return false;
}

void pocsag_pager_scene_send_host_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    pcsg_send_host_unload(app);
    widget_reset(app->widget);
}
