#include "flipper_wedge.h"
#include "helpers/flipper_wedge_debug.h"
#include "helpers/flipper_wedge_log.h"

bool flipper_wedge_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipperWedge* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

void flipper_wedge_tick_event_callback(void* context) {
    furi_assert(context);
    FlipperWedge* app = context;

    // Handle pending output mode switch (async to avoid blocking UI thread)
    if(app->output_switch_pending) {
        FURI_LOG_I(TAG, "Tick: Processing pending output mode switch");
        flipper_wedge_debug_log(TAG, "Tick callback executing deferred mode switch");

        flipper_wedge_switch_output_mode(app, app->output_switch_target);
        app->output_switch_pending = false;

        flipper_wedge_debug_log(TAG, "Deferred mode switch complete");
    }

    scene_manager_handle_tick_event(app->scene_manager);
}

//leave app if back button pressed
bool flipper_wedge_navigation_event_callback(void* context) {
    furi_assert(context);
    FlipperWedge* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

FlipperWedge* flipper_wedge_app_alloc() {
    FlipperWedge* app = malloc(sizeof(FlipperWedge));

    // Initialize debug logging to SD card
    flipper_wedge_debug_init();
    flipper_wedge_debug_log("App", "=== APP STARTING ===");

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    //Turn backlight on, believe me this makes testing your app easier
    notification_message(app->notification, &sequence_display_backlight_on);

    //Scene additions
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&flipper_wedge_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipper_wedge_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flipper_wedge_tick_event_callback, 100);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipper_wedge_custom_event_callback);
    app->submenu = submenu_alloc();

    // Set defaults
    app->output_mode = FlipperWedgeOutputUsb;  // Default: USB HID
    app->usb_debug_mode = false;  // Deprecated: kept for backward compatibility

    // Scanning defaults
    app->mode = FlipperWedgeModeNfc;  // Default: NFC only
    app->mode_startup_behavior = FlipperWedgeModeStartupRemember;  // Default: Remember last mode
    app->scan_state = FlipperWedgeScanStateIdle;
    app->delimiter[0] = '\0';  // Empty delimiter by default
    app->append_enter = true;
    app->vibration_level = FlipperWedgeVibrationMedium;  // Default: Medium vibration
    app->ndef_max_len = FlipperWedgeNdefMaxLen250;  // Default: 250 char limit (fast typing)
    app->log_to_sd = false;  // Default: Logging disabled for privacy/performance
    app->restart_pending = false;  // Deprecated field, no longer used
    app->output_switch_pending = false;
    app->output_switch_target = FlipperWedgeOutputUsb;

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';
    app->output_buffer[0] = '\0';

    // Used for File Browser
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->file_path = furi_string_alloc();

    // Allocate keyboard layout (default to QWERTY)
    app->keyboard_layout = flipper_wedge_keyboard_layout_alloc();

    // Load configs BEFORE initializing HID (so we respect output_mode setting)
    // This also loads keyboard layout settings
    flipper_wedge_read_settings(app);

    // Allocate HID worker (manages HID interface in separate thread)
    app->hid_worker = flipper_wedge_hid_worker_alloc();

    // Start HID worker with loaded output mode (like Bad USB pattern)
    flipper_wedge_debug_log("App", "Starting HID worker in %s mode",
                        app->output_mode == FlipperWedgeOutputUsb ? "USB" : "BLE");
    FlipperWedgeHidWorkerMode worker_mode = (app->output_mode == FlipperWedgeOutputUsb) ?
        FlipperWedgeHidWorkerModeUsb : FlipperWedgeHidWorkerModeBle;
    flipper_wedge_hid_worker_start(app->hid_worker, worker_mode);

    // Allocate NFC module
    app->nfc = flipper_wedge_nfc_alloc();

    // Allocate RFID module
    app->rfid = flipper_wedge_rfid_alloc();

    // Timers will be created as needed
    app->timeout_timer = NULL;
    app->display_timer = NULL;

    view_dispatcher_add_view(
        app->view_dispatcher, FlipperWedgeViewIdMenu, submenu_get_view(app->submenu));
    app->flipper_wedge_startscreen = flipper_wedge_startscreen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperWedgeViewIdStartscreen,
        flipper_wedge_startscreen_get_view(app->flipper_wedge_startscreen));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperWedgeViewIdSettings,
        variable_item_list_get_view(app->variable_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperWedgeViewIdTextInput,
        text_input_get_view(app->text_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperWedgeViewIdNumberInput,
        number_input_get_view(app->number_input));

    //End Scene Additions

    return app;
}

void flipper_wedge_switch_output_mode(FlipperWedge* app, FlipperWedgeOutput new_mode) {
    furi_assert(app);

    // Note: app->output_mode may already equal new_mode (set by settings callback)
    // but we still need to restart the HID worker with the new profile
    FURI_LOG_I(TAG, "Switching output mode to: %d", new_mode);
    flipper_wedge_debug_log(TAG, "=== OUTPUT MODE SWITCH: %d -> %d ===",
                        app->output_mode, new_mode);

    // STEP 1: Stop all workers (NFC/RFID)
    flipper_wedge_debug_log(TAG, "Step 1: Stopping NFC/RFID workers");
    bool nfc_was_scanning = flipper_wedge_nfc_is_scanning(app->nfc);
    bool rfid_was_scanning = flipper_wedge_rfid_is_scanning(app->rfid);
    bool parse_ndef = (app->mode == FlipperWedgeModeNdef);

    if(nfc_was_scanning) {
        flipper_wedge_nfc_stop(app->nfc);
        flipper_wedge_debug_log(TAG, "NFC stopped");
    }
    if(rfid_was_scanning) {
        flipper_wedge_rfid_stop(app->rfid);
        flipper_wedge_debug_log(TAG, "RFID stopped");
    }

    // STEP 2: Stop HID worker (deinits HID in worker thread, waits for exit)
    flipper_wedge_debug_log(TAG, "Step 2: Stopping HID worker (old mode=%s)",
                        app->output_mode == FlipperWedgeOutputUsb ? "USB" : "BLE");
    flipper_wedge_hid_worker_stop(app->hid_worker);
    flipper_wedge_debug_log(TAG, "HID worker stopped");

    // STEP 3: Small delay between modes (safety buffer)
    flipper_wedge_debug_log(TAG, "Step 3: Waiting 300ms before starting new mode");
    furi_delay_ms(300);

    // STEP 4: Switch mode
    flipper_wedge_debug_log(TAG, "Step 4: Switching mode");
    app->output_mode = new_mode;

    // STEP 5: Start HID worker with new mode (inits HID in worker thread)
    flipper_wedge_debug_log(TAG, "Step 5: Starting HID worker (new mode=%s)",
                        new_mode == FlipperWedgeOutputUsb ? "USB" : "BLE");
    FlipperWedgeHidWorkerMode worker_mode = (new_mode == FlipperWedgeOutputUsb) ?
        FlipperWedgeHidWorkerModeUsb : FlipperWedgeHidWorkerModeBle;
    flipper_wedge_hid_worker_start(app->hid_worker, worker_mode);
    flipper_wedge_debug_log(TAG, "HID worker started");

    // STEP 6: Restart NFC/RFID workers if they were running
    flipper_wedge_debug_log(TAG, "Step 6: Restarting NFC/RFID workers (NFC=%d, RFID=%d)",
                        nfc_was_scanning, rfid_was_scanning);
    if(nfc_was_scanning) {
        flipper_wedge_nfc_start(app->nfc, parse_ndef);
        flipper_wedge_debug_log(TAG, "NFC restarted");
    }
    if(rfid_was_scanning) {
        flipper_wedge_rfid_start(app->rfid);
        flipper_wedge_debug_log(TAG, "RFID restarted");
    }

    // STEP 7: Save settings to persist the change
    flipper_wedge_debug_log(TAG, "Step 7: Saving settings");
    flipper_wedge_save_settings(app);

    FURI_LOG_I(TAG, "Output mode switch complete");
    flipper_wedge_debug_log(TAG, "=== OUTPUT MODE SWITCH COMPLETE ===");
}

void flipper_wedge_app_free(FlipperWedge* app) {
    furi_assert(app);

    // Free timers
    if(app->timeout_timer) {
        furi_timer_free(app->timeout_timer);
        app->timeout_timer = NULL;
    }
    if(app->display_timer) {
        furi_timer_free(app->display_timer);
        app->display_timer = NULL;
    }

    // Free RFID module
    if(app->rfid) {
        flipper_wedge_rfid_free(app->rfid);
        app->rfid = NULL;
    }

    // Free NFC module
    if(app->nfc) {
        flipper_wedge_nfc_free(app->nfc);
        app->nfc = NULL;
    }

    // Free HID worker (stops thread and cleans up HID)
    flipper_wedge_hid_worker_free(app->hid_worker);

    // Free keyboard layout
    if(app->keyboard_layout) {
        flipper_wedge_keyboard_layout_free(app->keyboard_layout);
        app->keyboard_layout = NULL;
    }

    // Scene manager
    scene_manager_free(app->scene_manager);

    // View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdStartscreen);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    flipper_wedge_startscreen_free(app->flipper_wedge_startscreen);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdTextInput);
    text_input_free(app->text_input);

    view_dispatcher_free(app->view_dispatcher);

    // Restore backlight to auto mode before closing notification service
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    app->gui = NULL;
    app->notification = NULL;

    // Close File Browser
    furi_record_close(RECORD_DIALOGS);
    furi_string_free(app->file_path);

    // Close debug logging
    flipper_wedge_debug_log("App", "=== APP EXITING ===");
    flipper_wedge_debug_close();

    // Close scan logging (free mutex)
    flipper_wedge_log_close();

    //Remove whatever is left
    free(app);
}

int32_t flipper_wedge_app(void* p) {
    UNUSED(p);
    FlipperWedge* app = flipper_wedge_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(
        app->scene_manager, FlipperWedgeSceneStartscreen); //Start with start screen

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(app->view_dispatcher);

    flipper_wedge_save_settings(app);

    furi_hal_power_suppress_charge_exit();
    flipper_wedge_app_free(app);

    return 0;
}
