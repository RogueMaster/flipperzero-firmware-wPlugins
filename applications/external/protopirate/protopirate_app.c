// protopirate_app.c
#include "protopirate_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include "protocols/protocol_items.h"
#include "helpers/protopirate_settings.h"
#include "helpers/protopirate_storage.h"
#include "protocols/keys.h"
#include <string.h>

#define TAG           "ProtoPirateApp"
#define LOG_HEAP(msg) FURI_LOG_I(TAG, "%s - Free heap: %zu", msg, memmgr_get_free_heap())

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

bool protopirate_ensure_variable_item_list(ProtoPirateApp* app) {
    furi_check(app);
    if(app->variable_item_list) {
        return true;
    }

    app->variable_item_list = variable_item_list_alloc();
    if(!app->variable_item_list) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    return true;
}

bool protopirate_ensure_widget(ProtoPirateApp* app) {
    furi_check(app);
    if(app->widget) {
        return true;
    }

    app->widget = widget_alloc();
    if(!app->widget) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, ProtoPirateViewWidget, widget_get_view(app->widget));
    return true;
}

bool protopirate_ensure_text_input(ProtoPirateApp* app) {
    furi_check(app);
    if(app->text_input) {
        return true;
    }

    app->text_input = text_input_alloc();
    if(!app->text_input) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher, ProtoPirateViewTextInput, text_input_get_view(app->text_input));
    return true;
}

bool protopirate_ensure_view_about(ProtoPirateApp* app) {
    furi_check(app);
    if(app->view_about) {
        return true;
    }

    app->view_about = view_alloc();
    if(!app->view_about) {
        return false;
    }

    view_dispatcher_add_view(app->view_dispatcher, ProtoPirateViewAbout, app->view_about);
    return true;
}

bool protopirate_ensure_receiver_view(ProtoPirateApp* app) {
    furi_check(app);
    if(app->protopirate_receiver) {
        return true;
    }

    app->protopirate_receiver = protopirate_view_receiver_alloc(app->auto_save);
    if(!app->protopirate_receiver) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewReceiver,
        protopirate_view_receiver_get_view(app->protopirate_receiver));
    return true;
}

static void protopirate_radio_init_cleanup(ProtoPirateApp* app, bool devices_initialized) {
    furi_check(app);
    furi_check(app->txrx);

    if(app->txrx->receiver) {
        subghz_receiver_free(app->txrx->receiver);
        app->txrx->receiver = NULL;
    }

    if(app->txrx->radio_device) {
        if(devices_initialized) {
            subghz_devices_idle(app->txrx->radio_device);
        }
        radio_device_loader_end(app->txrx->radio_device);
        app->txrx->radio_device = NULL;
    }

    if(app->txrx->environment) {
        subghz_environment_free(app->txrx->environment);
        app->txrx->environment = NULL;
    }

    if(devices_initialized) {
        subghz_devices_deinit();
    }

    app->txrx->protocol_registry = NULL;
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->radio_initialized = false;
}

ProtoPirateApp* protopirate_app_alloc() {
    LOG_HEAP("Pre alloc");
    protopirate_storage_purge_temp_history_at_startup();
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate ProtoPirateApp app !");
        return NULL;
    }
    memset(app, 0, sizeof(ProtoPirateApp));

    LOG_HEAP("Start alloc");
    FURI_LOG_I(TAG, "Allocating ProtoPirate Decoder App");

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    LOG_HEAP("After GUI");

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
    LOG_HEAP("After ViewDispatcher");

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
    memset(app->save_filename, 0, sizeof(app->save_filename));

    // File Browser path
    app->file_path = furi_string_alloc();
    furi_string_set(app->file_path, PROTOPIRATE_APP_FOLDER);

    LOG_HEAP("After basic views");

    // Load saved settings
    ProtoPirateSettings settings;
    protopirate_settings_load(&settings);

    // Apply auto-save setting
    app->auto_save = settings.auto_save;
    app->tx_power = settings.tx_power;

    // Init setting - KEEP THIS, it's small
    app->setting = subghz_setting_alloc();
    app->loaded_file_path = NULL;
    app->start_tx_time = 0;
    app->psa_bf_state = NULL;
    app->psa_bf_thread = NULL;
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

    LOG_HEAP("After subghz_setting");

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

    app->txrx->preset = malloc(sizeof(SubGhzRadioPreset));
    app->txrx->preset->name = furi_string_alloc();
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->txrx->rx_key_state = ProtoPirateRxKeyStateIDLE;

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

    LOG_HEAP("After txrx basic setup");

    app->radio_initialized = false;

    LOG_HEAP("App alloc complete (radio deferred)");

    return app;
}

bool protopirate_radio_init(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    FURI_LOG_I(TAG, "=== protopirate_radio_init called ===");
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);

    if(app->radio_initialized) {
        const bool radio_ready = (app->txrx->environment != NULL) &&
                                 (app->txrx->radio_device != NULL);
        if(radio_ready) {
            FURI_LOG_D(TAG, "Radio already initialized, returning true");
            return true;
        }

        FURI_LOG_W(
            TAG,
            "Radio marked initialized but resources missing (env=%p device=%p), repairing",
            app->txrx->environment,
            app->txrx->radio_device);
        protopirate_radio_deinit(app);
    }

    // Fresh radio init - nothing was initialized before
    FURI_LOG_I(TAG, "Fresh radio init - allocating all components");
    LOG_HEAP("Radio init start");

    // Create environment with our custom protocols
    app->txrx->environment = subghz_environment_alloc();
    if(!app->txrx->environment) {
        FURI_LOG_E(TAG, "Failed to allocate environment!");
        protopirate_radio_init_cleanup(app, false);
        return false;
    }
    LOG_HEAP("After environment alloc");

    app->txrx->protocol_registry = NULL;

    if(!protopirate_refresh_protocol_registry(app, false)) {
        FURI_LOG_E(TAG, "Failed to configure protocol registry");
        protopirate_radio_init_cleanup(app, false);
        return false;
    }

    // Load keystores
    subghz_environment_load_keystore(app->txrx->environment, PROTOPIRATE_KEYSTORE_DIR_NAME);
    LOG_HEAP("After keystore load");

    // Load ProtoPirate specific keys
    protopirate_keys_load(app->txrx->environment);
    FURI_LOG_I(TAG, "Loaded ProtoPirate secure keys");
    LOG_HEAP("After keys load");

    // Initialize SubGhz devices
    subghz_devices_init();
    FURI_LOG_D(TAG, "SubGhz devices initialized");

    // Try external CC1101 first
    app->txrx->radio_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeExternalCC1101);

    // if not loading, fallback to internal
    if(!app->txrx->radio_device) {
        FURI_LOG_W(TAG, "External CC1101 not found, trying internal radio");
        app->txrx->radio_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeInternal);
    }

    if(!app->txrx->radio_device) {
        FURI_LOG_E(TAG, "Failed to initialize any radio device!");
        protopirate_radio_init_cleanup(app, true);
        return false;
    }
#ifndef REMOVE_LOGS
    const char* device_name = subghz_devices_get_name(app->txrx->radio_device);
    bool is_external = device_name && strstr(device_name, "ext");
    FURI_LOG_I(
        TAG,
        "Radio device initialized: %s (%s)",
        device_name ? device_name : "unknown",
        is_external ? "external" : "internal");
#endif
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);

    app->radio_initialized = true;

    FURI_LOG_D(TAG, "Final state: radio_initialized=%d", app->radio_initialized);

    LOG_HEAP("Radio init complete");

    return true;
}

// Deinitialize radio subsystem
void protopirate_radio_deinit(ProtoPirateApp* app) {
    FURI_LOG_I(TAG, "=== protopirate_radio_deinit called ===");
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);
    FURI_LOG_D(
        TAG,
        "Pointers: worker=%p, environment=%p, receiver=%p, history=%p, radio_device=%p",
        app->txrx->worker,
        app->txrx->environment,
        app->txrx->receiver,
        app->txrx->history,
        app->txrx->radio_device);

    bool has_radio_resources = app->radio_initialized || app->txrx->worker ||
                               app->txrx->environment || app->txrx->receiver ||
                               app->txrx->history || app->txrx->radio_device;
    if(!has_radio_resources) {
        FURI_LOG_D(TAG, "Radio resources were not initialized, returning");
        return;
    }

    LOG_HEAP("Radio deinit start");
    bool devices_initialized = app->radio_initialized || (app->txrx->radio_device != NULL);

    // Make sure we're not receiving
    if(app->txrx->worker && app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        FURI_LOG_D(TAG, "Stopping active RX, state=%d", app->txrx->txrx_state);
        subghz_worker_stop(app->txrx->worker);
        if(app->txrx->radio_device) {
            subghz_devices_stop_async_rx(app->txrx->radio_device);
        }
    }

    if(app->txrx->radio_device) {
        FURI_LOG_D(TAG, "Putting radio device to sleep and ending: %p", app->txrx->radio_device);
        subghz_devices_sleep(app->txrx->radio_device);
        radio_device_loader_end(app->txrx->radio_device);
        app->txrx->radio_device = NULL;
    } else {
        FURI_LOG_D(TAG, "Radio device was NULL, skipping sleep/end");
    }

    if(devices_initialized) {
        FURI_LOG_D(TAG, "Calling subghz_devices_deinit");
        subghz_devices_deinit();
    }

    if(app->txrx->receiver) {
        FURI_LOG_D(TAG, "Freeing receiver %p", app->txrx->receiver);
        subghz_receiver_free(app->txrx->receiver);
        app->txrx->receiver = NULL;
    } else {
        FURI_LOG_D(TAG, "Receiver was NULL, skipping free");
    }

    if(app->txrx->environment) {
        FURI_LOG_D(TAG, "Freeing environment %p", app->txrx->environment);
        subghz_environment_free(app->txrx->environment);
        app->txrx->environment = NULL;
        app->txrx->protocol_registry = NULL;
    } else {
        FURI_LOG_D(TAG, "Environment was NULL, skipping free");
    }

    if(app->txrx->history) {
        FURI_LOG_D(TAG, "Freeing history %p", app->txrx->history);
        protopirate_history_free(app->txrx->history);
        app->txrx->history = NULL;
    } else {
        FURI_LOG_D(TAG, "History was NULL, skipping free");
    }

    if(app->txrx->worker) {
        FURI_LOG_D(TAG, "Freeing worker %p", app->txrx->worker);
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    } else {
        FURI_LOG_D(TAG, "Worker was NULL, skipping free");
    }

    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->radio_initialized = false;

    FURI_LOG_D(TAG, "Final state: radio_initialized=%d", app->radio_initialized);
    LOG_HEAP("Radio deinit complete");
}

void protopirate_app_free(ProtoPirateApp* app) {
    furi_check(app);

    FURI_LOG_I(TAG, "=== protopirate_app_free called ===");
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);

    // Save settings before exiting
    ProtoPirateSettings settings;
    settings.frequency = app->txrx->preset->frequency;
    settings.auto_save = app->auto_save;
    settings.tx_power = app->tx_power;
    settings.hopping_enabled = (app->txrx->hopper_state != ProtoPirateHopperStateOFF);

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
        "Saving settings: freq=%lu, preset=%u, auto_save=%d, hopping=%d",
        settings.frequency,
        settings.preset_index,
        settings.auto_save,
        settings.hopping_enabled);

    protopirate_settings_save(&settings);

    // Deinitialize whichever is active - NULL checks inside handle all cases
    FURI_LOG_D(TAG, "Calling radio_deinit");
    protopirate_radio_deinit(app);

    if(app->loaded_file_path) {
        FURI_LOG_D(TAG, "Freeing loaded_file_path");
        furi_string_free(app->loaded_file_path);
        app->loaded_file_path = NULL;
    }

    // Submenu
    if(app->submenu) {
        FURI_LOG_D(TAG, "Removing submenu view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewSubmenu);
        submenu_free(app->submenu);
    }

    // Variable Item List
    if(app->variable_item_list) {
        FURI_LOG_D(TAG, "Removing variable_item_list view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
    }

    // About View
    if(app->view_about) {
        FURI_LOG_D(TAG, "Removing about view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewAbout);
        view_free(app->view_about);
    }

    // File path
    if(app->file_path) {
        FURI_LOG_D(TAG, "Freeing file_path");
        furi_string_free(app->file_path);
    }

    // Widget
    if(app->widget) {
        FURI_LOG_D(TAG, "Removing widget view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewWidget);
        widget_free(app->widget);
    }

    // Text Input
    if(app->text_input) {
        FURI_LOG_D(TAG, "Removing text_input view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewTextInput);
        text_input_free(app->text_input);
    }
    if(app->save_protocol) {
        furi_string_free(app->save_protocol);
        app->save_protocol = NULL;
    }

    // Receiver
    if(app->protopirate_receiver) {
        FURI_LOG_D(TAG, "Removing receiver view");
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewReceiver);
        protopirate_view_receiver_free(app->protopirate_receiver);
    }

    if(app->psa_bf_thread) {
        if(app->psa_bf_state) app->psa_bf_state->cancel = 1;
        furi_thread_join(app->psa_bf_thread);
        furi_thread_free(app->psa_bf_thread);
        app->psa_bf_thread = NULL;
    }
    if(app->psa_bf_state) {
        free(app->psa_bf_state);
        app->psa_bf_state = NULL;
    }

    // Setting
    FURI_LOG_D(TAG, "Freeing subghz_setting");
    subghz_setting_free(app->setting);

    // Free preset
    FURI_LOG_D(TAG, "Freeing preset");
    furi_string_free(app->txrx->preset->name);
    free(app->txrx->preset);

    free(app->txrx);

    // View dispatcher
    FURI_LOG_D(TAG, "Freeing view_dispatcher and scene_manager");
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close Dialogs
    FURI_LOG_D(TAG, "Closing dialogs record");
    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    // Notifications
    FURI_LOG_D(TAG, "Closing notifications record");
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    FURI_LOG_D(TAG, "Closing GUI record");
    furi_record_close(RECORD_GUI);

    FURI_LOG_I(TAG, "App free complete");
    free(app);
}

int32_t protopirate_app(char* p) {
    furi_hal_power_suppress_charge_enter();

    ProtoPirateApp* protopirate_app = protopirate_app_alloc();
    if(!protopirate_app) {
        // logging is already done in protopirate_app_alloc()
        furi_hal_power_suppress_charge_exit();
        return -1;
    }

    // Handle Command line PSF that may have been passed to us
    bool load_saved = (p && strlen(p));
    if(load_saved) protopirate_app->loaded_file_path = furi_string_alloc_set(p);
    scene_manager_next_scene(
        protopirate_app->scene_manager,
        (load_saved) ? ProtoPirateSceneSavedInfo : ProtoPirateSceneStart);

#ifdef ENABLE_EMULATE_FEATURE
    //We now jump straight to emulate scene from Browser. If the user wanted the key to look at, just click back.
    //Makes it faster in my use case
    if(load_saved) {
        view_dispatcher_send_custom_event(
            protopirate_app->view_dispatcher, ProtoPirateCustomEventSavedInfoEmulate);
        notification_message(protopirate_app->notifications, &sequence_success);
    }
#else
    //We now jump straight to emulate scene from Browser. If the user wanted the key to look at, just click back.
    //Makes it faster in my use case
    if(load_saved) {
        view_dispatcher_send_custom_event(
            protopirate_app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSave);
    }
#endif

    view_dispatcher_run(protopirate_app->view_dispatcher);

    protopirate_app_free(protopirate_app);

    furi_hal_power_suppress_charge_exit();

    return 0;
}
