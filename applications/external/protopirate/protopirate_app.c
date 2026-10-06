// protopirate_app.c
#include "protopirate_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include "helpers/protopirate_settings.h"
#include "helpers/protopirate_storage.h"
#include "helpers/protopirate_bruteforce_host.h"
#include "helpers/protopirate_views.h"
#include "helpers/protopirate_radio.h"
#include <string.h>

#define TAG "PPApp"

static bool protopirate_app_custom_event_callback(void* context, uint32_t event) {
    ProtoPirateApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool protopirate_app_back_event_callback(void* context) {
    ProtoPirateApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void protopirate_app_tick_event_callback(void* context) {
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

    // Null out plugin pointers just in case. (Should be done my memset above.)
    /*app->running_plugin_flipper_application = NULL;
    app->running_plugin.plugin_pointer = NULL;
    app->running_bruteforce_plugin.plugin_pointer = NULL;
    app->variable_item_list = NULL;
    app->text_input = NULL;
    app->save_history_idx = 0;
    app->emulate_disabled_for_loaded = false;
    app->save_filename = NULL;
    */

    FURI_LOG_I(TAG, "Allocating ProtoPirate Decoder App");

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
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

    // File Browser path
    app->file_path = malloc(strlen(PROTOPIRATE_APP_FOLDER) + 1);
    snprintf(app->file_path, strlen(PROTOPIRATE_APP_FOLDER) + 1, PROTOPIRATE_APP_FOLDER);

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
    memset(app->txrx, 0, sizeof(ProtoPirateTxRx));
    //app->txrx->running_plugin.plugin_pointer = NULL; //memset shoulnt need it,
    app->txrx->preset = malloc(sizeof(SubGhzRadioPreset));
    app->txrx->preset->name = furi_string_alloc();
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->txrx->rx_key_state = ProtoPirateRxKeyStateIDLE;
    app->txrx->protocol_registry_route = ProtoPirateProtocolRegistryRouteAMDefault;

    // Get preset name and data
    const char* preset_name = subghz_setting_get_preset_name(app->setting, preset_index);
    uint8_t* preset_data = subghz_setting_get_preset_data(app->setting, preset_index);
    size_t preset_data_size = subghz_setting_get_preset_data_size(app->setting, preset_index);

    FURI_LOG_I(
        TAG,
        "Settings: freq=%lu, preset=%s, auto_save=%d, hopping=%lu",
        frequency,
        preset_name,
        settings.auto_save,
        settings.hopper_state);

    //Load the models database, get the count of the models for the list.
#ifdef ENABLE_MODELS_DATABASE
    if(shared_plugin_load(
           &app->running_plugin_flipper_application,
           &app->running_plugin,
           ProtoPirateSharedPluginsConfig,
           NULL) &&
       app->running_plugin.config_plugin) {
        FURI_LOG_D("test", "getting count");
        app->car_models_count = app->running_plugin.config_plugin->car_model_get_count();
        FURI_LOG_D("test", "got count");
    } else {
        notification_message(app->notifications, &sequence_error);
        app->car_models_count = 0;
    }
    app->selected_model = malloc(sizeof(ProtoPirateCarModel));
    app->selected_model->name = NULL;
    app->selected_model->preset = NULL; // important initialization
    app->selected_model->index = 0; // optional but clean

    //Grab selected car model.
    if(app->running_plugin.config_plugin) {
        if(settings.car_model_index) {
            //Get the selected car model.
            app->running_plugin.config_plugin->car_model_get_by_index(
                app->selected_model, settings.car_model_index, app->car_models_count, app->setting);
            app->selected_model->last_preset_index = settings.preset_index;

            //Preset for the selected model...
            protopirate_preset_init(
                app,
                furi_string_get_cstr(app->selected_model->preset->name),
                app->selected_model->preset->frequency,
                app->selected_model->preset->data,
                app->selected_model->preset->data_size);
        } else {
            //This will return Select a model or No Models in Database
            app->running_plugin.config_plugin->car_model_get_by_index(
                app->selected_model, 0, app->car_models_count, app->setting);

            //Preset set in Config.
            protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size);
        }

        //Kill the config plugin now.
        shared_plugin_unload(&app->running_plugin_flipper_application, &app->running_plugin);
    } else {
#endif
        //Preset set in Config.
        protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size);
#ifdef ENABLE_MODELS_DATABASE
    }
#endif

    // Apply hopping state from settings
    if(settings.hopper_state) {
        app->txrx->hopper_state = ProtoPirateHopperStateRunning;
        app->txrx->hopper_rssi = settings.hopper_state;
    } else {
        app->txrx->hopper_state = ProtoPirateHopperStateOFF;
        app->txrx->hopper_rssi = 0;
    }
    app->txrx->hopper_idx_frequency = 0;
    app->txrx->hopper_timeout = 0;
    app->txrx->idx_menu_chosen = 0;

    app->radio_initialized = false;

    return app;
}

void protopirate_app_free(ProtoPirateApp* app) {
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
#ifdef ENABLE_EMULATE_FEATURE
    settings.emulate_feature_enabled = app->emulate_feature_enabled;
#else
    settings.emulate_feature_enabled = false;
#endif
    settings.hopper_state = (app->txrx->hopper_state == ProtoPirateHopperStateOFF) ?
                                ProtoPirateHopperStateOFF :
                                app->txrx->hopper_rssi;

    //Get the selected Model, and get the preset to save.
#ifdef ENABLE_MODELS_DATABASE
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
    if(app->selected_model->name) free(app->selected_model->name);
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
#endif

    FURI_LOG_I(
        TAG,
        "Saving settings: freq=%lu, preset=%u, auto_save=%d, hopping=%lu, emulate=%d",
        settings.frequency,
        settings.preset_index,
        settings.auto_save,
        settings.hopper_state,
        settings.emulate_feature_enabled);

    protopirate_settings_save(&settings);

    FURI_LOG_D(TAG, "Calling radio_deinit");
    protopirate_radio_deinit(app);

    if(app->loaded_file_path) {
        FURI_LOG_D(TAG, "Freeing loaded_file_path");
        free(app->loaded_file_path);
        app->loaded_file_path = NULL;
    }

    protopirate_views_free(app);

    if(app->save_filename) {
        free(app->save_filename);
        app->save_filename = NULL;
    }

    if(app->file_path) {
        FURI_LOG_D(TAG, "Freeing file_path");
        free(app->file_path);
        app->file_path = NULL;
    }

    protopirate_bruteforce_context_release(app);

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
    ProtoPirateApp* protopirate_app = protopirate_app_alloc();
    if(!protopirate_app) {
        return -1;
    }

    //Stop charging while running the app.
    furi_hal_power_suppress_charge_enter();

    // Handle Command line PSF that may have been passed to us
    bool load_saved = (p && strlen(p));
    if(load_saved) {
        protopirate_app->loaded_file_path = malloc(strlen(p) + 1);
        snprintf(protopirate_app->loaded_file_path, strlen(p) + 1, p);
    }

//We now jump straight to emulate scene from Browser.
#ifdef ENABLE_EMULATE_FEATURE
    scene_manager_next_scene(
        protopirate_app->scene_manager,
        (load_saved) ? ((protopirate_app->emulate_feature_enabled) ? ProtoPirateSceneEmulate :
                                                                     ProtoPirateSceneSavedInfo) :
                       ProtoPirateSceneStart);
#else
    scene_manager_next_scene(
        protopirate_app->scene_manager,
        (load_saved) ? ProtoPirateSceneSavedInfo : ProtoPirateSceneStart);
#endif
    //Pop up the beep if we are startng emulate.
    if(load_saved && protopirate_app->emulate_feature_enabled) {
        notification_message(protopirate_app->notifications, &sequence_success);
    }

    //Run the App
    view_dispatcher_run(protopirate_app->view_dispatcher);

    //Free the App and allow chargin again.
    protopirate_app_free(protopirate_app);

    //Restore Charging State
    furi_hal_power_suppress_charge_exit();
    return 0;
}
