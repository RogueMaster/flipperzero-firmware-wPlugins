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
    app->check_saved = settings.check_saved;
    app->tx_power = settings.tx_power;
#ifdef ENABLE_EMULATE_FEATURE
    app->emulate_feature_enabled = settings.emulate_feature_enabled;
#else
    app->emulate_feature_enabled = false;
#endif

    // Init setting - KEEP THIS, it's small
    app->setting = subghz_setting_alloc();
    app->loaded_file_path = NULL;
    app->start_tx_time = 0;
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

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

    protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size);

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
    settings.check_saved = app->check_saved;
    settings.tx_power = app->tx_power;
    settings.hopping_enabled = (app->txrx->hopper_state != ProtoPirateHopperStateOFF);
#ifdef ENABLE_EMULATE_FEATURE
    settings.emulate_feature_enabled = app->emulate_feature_enabled;
#else
    settings.emulate_feature_enabled = false;
#endif

    // Find current preset index
    settings.preset_index = 0;
    const char* current_preset = furi_string_get_cstr(app->txrx->preset->name);
    for(uint8_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
        if(strcmp(subghz_setting_get_preset_name(app->setting, i), current_preset) == 0) {
            settings.preset_index = i;
            break;
        }
    }

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
