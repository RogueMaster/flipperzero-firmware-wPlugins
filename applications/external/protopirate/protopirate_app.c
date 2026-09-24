// protopirate_app.c
#include "protopirate_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include "helpers/protopirate_settings.h"
#include "helpers/protopirate_storage.h"
#include "helpers/protopirate_psa_bf_host.h"
#include "helpers/protopirate_views.h"
#include "helpers/protopirate_radio.h"
#include <string.h>

#define TAG "ProtoPirateApp"

// -----------------------------------------------------------------------------
// Plugin load / unload
// -----------------------------------------------------------------------------
void config_plugin_unload(ProtoPirateApp* app) {
    furi_check(app);

    app->config_plugin = NULL;

    if(app->plugin_manager) {
        plugin_manager_free(app->plugin_manager);
        app->plugin_manager = NULL;
    }

    if(app->plugin_resolver) {
        composite_api_resolver_free(app->plugin_resolver);
        app->plugin_resolver = NULL;
    }
}

bool config_plugin_load(
    ProtoPirateApp* app,
    const ProtoPirateConfigSceneHostApi* protopirate_config_scene_host_api) {
    furi_check(app);

    if(app->config_plugin) return true;

    if(app->plugin_manager || app->plugin_resolver) {
        config_plugin_unload(app);
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate config plugin resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        PROTOPIRATE_CONFIG_PLUGIN_APP_ID,
        PROTOPIRATE_CONFIG_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate config plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    PluginManagerError error = plugin_manager_load_single(manager, CONFIG_PLUGIN_PATH);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load config plugin %s: %d", CONFIG_PLUGIN_PATH, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const ProtoPirateConfigPlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || !plugin->on_enter) {
        FURI_LOG_E(TAG, "Config plugin entry point is invalid");
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->plugin_resolver = resolver;
    app->plugin_manager = manager;
    app->config_plugin = plugin;
    plugin->set_host_api(protopirate_config_scene_host_api);
    return true;
}

static bool protopirate_app_custom_event_callback(void* context, uint32_t event) {
    furi_check(context);
    ProtoPirateApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool protopirate_app_back_event_callback(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void protopirate_app_tick_event_callback(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

ProtoPirateApp* protopirate_app_alloc() {
    protopirate_storage_purge_temp_history_at_startup();
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate ProtoPirateApp app !");
        return NULL;
    }
    memset(app, 0, sizeof(ProtoPirateApp));

    FURI_LOG_I(TAG, "Allocating ProtoPirate Decoder App");

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
#if defined(FW_ORIGIN_RM)
    view_dispatcher_enable_queue(app->view_dispatcher);
#endif
    app->scene_manager = scene_manager_alloc(&protopirate_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, protopirate_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, protopirate_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, protopirate_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Open Dialogs record
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ProtoPirateViewSubmenu, submenu_get_view(app->submenu));

    app->save_protocol = NULL;
    app->save_from_saved_info = false;
    app->save_history_idx = 0;
    app->emulate_disabled_for_loaded = false;
    memset(app->save_filename, 0, sizeof(app->save_filename));

    // File Browser path
    app->file_path = furi_string_alloc();
    furi_string_set(app->file_path, PROTOPIRATE_APP_FOLDER);

    // Load saved settings
    ProtoPirateSettings settings;
    protopirate_settings_load(&settings);

    // Apply auto-save setting
    app->auto_save = settings.auto_save;
    app->sound = settings.sound;
    app->check_saved = settings.check_saved;
    app->tx_power = settings.tx_power;
    app->datetime_filenames = settings.datetime_filenames;
#ifdef ENABLE_EMULATE_FEATURE
    app->emulate_feature_enabled = settings.emulate_feature_enabled;
#else
    app->emulate_feature_enabled = false;
#endif

    // Init setting - KEEP THIS, it's small
    app->setting = subghz_setting_alloc();
    app->loaded_file_path = NULL;
    app->start_tx_time = 0;
    app->deferred_storage_timer = NULL;
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user.txt"));

    // Apply loaded frequency and preset, with validation
    uint32_t frequency = settings.frequency;
    uint8_t preset_index = settings.preset_index;

    // Validate frequency
    bool frequency_valid = false;
    for(size_t i = 0; i < subghz_setting_get_frequency_count(app->setting); i++) {
        if(subghz_setting_get_frequency(app->setting, i) == frequency) {
            frequency_valid = true;
            break;
        }
    }
    if(!frequency_valid) {
        frequency = subghz_setting_get_default_frequency(app->setting);
        FURI_LOG_W(TAG, "Saved frequency invalid, using default: %lu", frequency);
    }

    // Validate preset index
    if(preset_index >= subghz_setting_get_preset_count(app->setting)) {
        preset_index = 0;
        FURI_LOG_W(TAG, "Saved preset index invalid, using default");
    }

    // Initialize TxRx structure with minimal setup
    app->lock = ProtoPirateLockOff;
    app->txrx = malloc(sizeof(ProtoPirateTxRx));
    furi_check(app->txrx);
    memset(app->txrx, 0, sizeof(ProtoPirateTxRx));

    app->txrx->preset = malloc(sizeof(SubGhzRadioPreset));
    furi_check(app->txrx->preset);
    app->txrx->preset->name = furi_string_alloc();
    furi_check(app->txrx->preset->name);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->txrx->rx_key_state = ProtoPirateRxKeyStateIDLE;
    app->txrx->protocol_registry_route = ProtoPirateProtocolRegistryRouteAMDefault;

    // Get preset name and data
    const char* preset_name = subghz_setting_get_preset_name(app->setting, preset_index);
    uint8_t* preset_data = subghz_setting_get_preset_data(app->setting, preset_index);
    size_t preset_data_size = subghz_setting_get_preset_data_size(app->setting, preset_index);

    FURI_LOG_I(
        TAG,
        "Settings: freq=%lu, preset=%s, auto_save=%d, hopping=%d",
        frequency,
        preset_name,
        settings.auto_save,
        settings.hopping_enabled);

    config_plugin_load(app, NULL);
    app->car_models_count = app->config_plugin->car_model_get_count();
    app->selected_model = malloc(sizeof(ProtoPirateCarModel));
    app->selected_model->name = furi_string_alloc();
    app->selected_model->preset = NULL; // important initialization
    app->selected_model->index = 0; // optional but clean
    app->variable_item_list = NULL;

    //Grab selected car model.
    if(settings.car_model_index) {
        app->config_plugin->car_model_get_by_index(
            app->selected_model, settings.car_model_index, app->car_models_count, app->setting);
        app->selected_model->last_preset_index = settings.preset_index;

        protopirate_preset_init(
            app,
            furi_string_get_cstr(app->selected_model->preset->name),
            app->selected_model->preset->frequency,
            app->selected_model->preset->data,
            app->selected_model->preset->data_size);
    } else {
        app->config_plugin->car_model_get_by_index(
            app->selected_model, 0, app->car_models_count, app->setting);

        protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size);
    }
    config_plugin_unload(app);

    // Apply hopping state from settings
    app->txrx->hopper_state = settings.hopping_enabled ? ProtoPirateHopperStateRunning :
                                                         ProtoPirateHopperStateOFF;
    app->txrx->hopper_idx_frequency = 0;
    app->txrx->hopper_timeout = 0;
    app->txrx->idx_menu_chosen = 0;

    app->radio_initialized = false;

    return app;
}

void protopirate_app_free(ProtoPirateApp* app) {
    furi_check(app);

    FURI_LOG_I(TAG, "=== protopirate_app_free called ===");
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);

    // Save settings before exiting
    ProtoPirateSettings settings;
    settings.frequency = app->txrx->preset->frequency;
    settings.auto_save = app->auto_save;
    settings.sound = app->sound;
    settings.check_saved = app->check_saved;
    settings.tx_power = app->tx_power;
    settings.datetime_filenames = app->datetime_filenames;
    settings.hopping_enabled = (app->txrx->hopper_state != ProtoPirateHopperStateOFF);
#ifdef ENABLE_EMULATE_FEATURE
    settings.emulate_feature_enabled = app->emulate_feature_enabled;
#else
    settings.emulate_feature_enabled = false;
#endif

    //Get the selected Model, and get the preset to save.
    if(app->selected_model && app->selected_model->index) {
        //Get Preset Index before model was selected.
        settings.car_model_index = app->selected_model->index;
        settings.preset_index = app->selected_model->last_preset_index;
    } else {
        // Find current preset index
        settings.car_model_index = 0; //Clear the selected model.
        settings.preset_index = 0;
        const char* current_preset = furi_string_get_cstr(app->txrx->preset->name);
        for(uint8_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
            if(strcmp(subghz_setting_get_preset_name(app->setting, i), current_preset) == 0) {
                settings.preset_index = i;
                break;
            }
        }
    }

    //Free the Model Name
    furi_string_free(app->selected_model->name);
    app->selected_model->index = 0;

    //Free the preset information.
    if(app->selected_model->preset) {
        //Free the preset data
        if((app->selected_model)->preset->data) {
            free(app->selected_model->preset->data);
            app->selected_model->preset->data = NULL;
        }

        //Free the Preset name
        furi_string_free(app->selected_model->preset->name);

        //Free the preset.
        free(app->selected_model->preset);
        app->selected_model->preset = NULL;
    }

    //Free the Model.
    free(app->selected_model);
    app->selected_model = NULL;

    FURI_LOG_I(
        TAG,
        "Saving settings: freq=%lu, preset=%u, auto_save=%d, hopping=%d, emulate=%d",
        settings.frequency,
        settings.preset_index,
        settings.auto_save,
        settings.hopping_enabled,
        settings.emulate_feature_enabled);

    protopirate_settings_save(&settings);

    protopirate_tool_scene_plugin_release(app);
#ifdef ENABLE_EMULATE_FEATURE
    protopirate_emulate_context_release(app);
#endif

    FURI_LOG_D(TAG, "Calling radio_deinit");
    protopirate_radio_deinit(app);

    if(app->loaded_file_path) {
        FURI_LOG_D(TAG, "Freeing loaded_file_path");
        furi_string_free(app->loaded_file_path);
        app->loaded_file_path = NULL;
    }

    protopirate_views_free(app);

    if(app->file_path) {
        FURI_LOG_D(TAG, "Freeing file_path");
        furi_string_free(app->file_path);
        app->file_path = NULL;
    }

    if(app->save_protocol) {
        furi_string_free(app->save_protocol);
        app->save_protocol = NULL;
    }

    protopirate_psa_bf_context_release(app);

    FURI_LOG_D(TAG, "Freeing subghz_setting");
    subghz_setting_free(app->setting);

    FURI_LOG_D(TAG, "Freeing preset");
    furi_string_free(app->txrx->preset->name);
    free(app->txrx->preset);

    free(app->txrx);

    FURI_LOG_D(TAG, "Freeing view_dispatcher and scene_manager");
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    FURI_LOG_D(TAG, "Closing dialogs record");
    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    FURI_LOG_D(TAG, "Closing notifications record");
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    FURI_LOG_D(TAG, "Closing GUI record");
    furi_record_close(RECORD_GUI);

    FURI_LOG_I(TAG, "App free complete");
    free(app);
}

int32_t protopirate_app(char* p) {
    furi_hal_power_suppress_charge_enter();

    ProtoPirateApp* protopirate_app = protopirate_app_alloc();
    if(!protopirate_app) {
        furi_hal_power_suppress_charge_exit();
        return -1;
    }

    // Handle Command line PSF that may have been passed to us
    bool load_saved = (p && strlen(p));
    if(load_saved) protopirate_app->loaded_file_path = furi_string_alloc_set(p);
    scene_manager_next_scene(
        protopirate_app->scene_manager,
        (load_saved) ? ProtoPirateSceneSavedInfo : ProtoPirateSceneStart);

    //We now jump straight to emulate scene from Browser. If the user wanted the key to look at, just click back.
    if(load_saved) {
#ifdef ENABLE_EMULATE_FEATURE
        if(protopirate_app->emulate_feature_enabled) {
            view_dispatcher_send_custom_event(
                protopirate_app->view_dispatcher, ProtoPirateCustomEventSavedInfoEmulate);
            notification_message(protopirate_app->notifications, &sequence_success);
        } else {
#endif
            view_dispatcher_send_custom_event(
                protopirate_app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSave);
#ifdef ENABLE_EMULATE_FEATURE
        }
#endif
    }

    view_dispatcher_run(protopirate_app->view_dispatcher);

    protopirate_app_free(protopirate_app);

    furi_hal_power_suppress_charge_exit();

    return 0;
}
