#ifdef PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_BUILD //These are excluded in FAM file, but heres a double catch!
#include "../../views/protopirate_remote_analyzer.h"
#include "../../protopirate_app_i.h"
#include "pp_ra_icons.h"

static const ProtoPirateSharedPluginHostApi* g_remote_analyzer_scene_host_api = NULL;
static ProtoPirateRemoteAnalyzer* g_remote_analyzer = NULL;

#define TAG "PPSRemoteAnalyzerPlugin"

void protopirate_scene_remote_analyzer_callback(ProtoPirateCustomEvent event, void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void plugin_protopirate_scene_remote_analyzer_on_enter(ProtoPirateApp* app) {
    FURI_LOG_D("TAG", "Setting Up View");
    g_remote_analyzer = protopirate_remote_analyzer_alloc(app->txrx);
    protopirate_remote_analyzer_set_callback(
        g_remote_analyzer, protopirate_scene_remote_analyzer_callback, app);
    protopirate_remote_analyzer_feedback_level(
        g_remote_analyzer, ProtoPirateRemoteAnalyzerFeedbackLevelAll, true);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewRemoteAnalyzer,
        protopirate_remote_analyzer_get_view(g_remote_analyzer));

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewRemoteAnalyzer);
}

bool plugin_protopirate_scene_remote_analyzer_on_event(
    ProtoPirateApp* app,
    SceneManagerEvent event) {
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSceneSettingLock) {
            notification_message(app->notifications, &sequence_set_green_255);
            switch(protopirate_remote_analyzer_feedback_level(
                g_remote_analyzer, ProtoPirateRemoteAnalyzerFeedbackLevelAll, false)) {
            case ProtoPirateRemoteAnalyzerFeedbackLevelAll:
                notification_message(app->notifications, &sequence_success);
                break;
            case ProtoPirateRemoteAnalyzerFeedbackLevelVibro:
                notification_message(app->notifications, &sequence_single_vibro);
                break;
            case ProtoPirateRemoteAnalyzerFeedbackLevelMute:
                break;
            }
            notification_message(app->notifications, &sequence_display_backlight_on);
            return true;
        } else if(event.event == ProtoPirateCustomEventViewReceiverUnlock) {
            notification_message(app->notifications, &sequence_reset_rgb);
            return true;
        } else if(event.event == ProtoPirateCustomEventViewReceiverOK) {
            // Don't need to save, we already saved on short event (and on exit event too)
            //subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
            uint32_t frequency =
                protopirate_remote_analyzer_get_frequency_to_save(g_remote_analyzer);
            bool is_am = protopirate_remote_analyzer_get_is_am_to_save(g_remote_analyzer);

            if(!frequency) return false;
            app->txrx->preset->frequency = frequency;
            app->txrx->hopper_state = ProtoPirateHopperStateOFF;
            app->txrx->hopper_rssi = 0;

            //We need to clear the selected model if there is one.
#ifdef ENABLE_MODELS_DATABASE
            if(app->selected_model->index > 0) {
                //Load the Plugin for the models.
                FlipperApplication* config_fal = NULL;
                ProtoPirateConfigPlugin* config_plugin = NULL;
                if(g_remote_analyzer_scene_host_api->plugin_load(
                       (void**)&config_fal,
                       (const void**)&config_plugin,
                       ProtoPirateSharedPluginsConfig,
                       NULL) &&
                   config_plugin) {
                    config_plugin->car_model_get_by_index(
                        app->selected_model, 0, app->car_models_count, app->setting);

                    //Restore last preset before model was selected...
                    /*protopirate_preset_init(
                        app,
                        subghz_setting_get_preset_name(
                            app->setting, app->selected_model->last_preset_index),
                        app->txrx->preset->frequency,
                        subghz_setting_get_preset_data(
                            app->setting, app->selected_model->last_preset_index),
                        subghz_setting_get_preset_data_size(
                            app->setting, app->selected_model->last_preset_index));
*/
                    g_remote_analyzer_scene_host_api->plugin_unload(
                        (void**)&config_fal, (const void**)config_plugin);
                } else {
                    notification_message(app->notifications, &sequence_error);
                }
            }
#endif

            if(is_am) {
                g_remote_analyzer_scene_host_api->preset_init(
                    app,
                    subghz_setting_get_preset_name(app->setting, 1),
                    app->txrx->preset->frequency,
                    subghz_setting_get_preset_data(app->setting, 1),
                    subghz_setting_get_preset_data_size(app->setting, 2));
            } else {
                g_remote_analyzer_scene_host_api->preset_init(
                    app,
                    subghz_setting_get_preset_name(app->setting, 3),
                    app->txrx->preset->frequency,
                    subghz_setting_get_preset_data(app->setting, 3),
                    subghz_setting_get_preset_data_size(app->setting, 3));
            }

            //Preset set in Config.
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventPluginNavigateSwitchToReceiver);
            return true;
        }
    }
    return false;
}

void plugin_protopirate_scene_remote_analyzer_on_exit(ProtoPirateApp* app) {
    notification_message(app->notifications, &sequence_reset_rgb);

    //Free the Frequency Analyzer
    if(g_remote_analyzer) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewSubmenu);
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewRemoteAnalyzer);
        protopirate_remote_analyzer_free(g_remote_analyzer);
        g_remote_analyzer = NULL;
    }
}

void remote_analyzer_plugin_set_host_api(const ProtoPirateSharedPluginHostApi* host_api) {
    g_remote_analyzer_scene_host_api = host_api;
}

static const ProtoPirateSharedPlugin protopirate_remote_analyzer_plugin = {
    .plugin_name = "",
    .on_enter = plugin_protopirate_scene_remote_analyzer_on_enter,
    .on_event = plugin_protopirate_scene_remote_analyzer_on_event,
    .on_exit = plugin_protopirate_scene_remote_analyzer_on_exit,
    .set_host_api = remote_analyzer_plugin_set_host_api,
};

static const FlipperAppPluginDescriptor protopirate_remote_analyzer_plugin_descriptor = {
    .appid = PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_API_VERSION,
    .entry_point = &protopirate_remote_analyzer_plugin,
};

const FlipperAppPluginDescriptor* protopirate_remote_analyzer_plugin_ep(void) {
    return &protopirate_remote_analyzer_plugin_descriptor;
}
#endif
