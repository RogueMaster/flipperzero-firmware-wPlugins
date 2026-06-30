#include "../flipper_wedge.h"
#include "../helpers/flipper_wedge_keyboard_layout.h"
#include <lib/toolbox/value_index.h>
#include <notification/notification_messages.h>

enum SettingsIndex {
    SettingsIndexHeader,
    SettingsIndexOutput,
    SettingsIndexBtPair,
    SettingsIndexDelimiter,
    SettingsIndexAppendEnter,
    SettingsIndexModeStartup,
    SettingsIndexVibration,
    SettingsIndexNdefMaxLen,
    SettingsIndexLogToSd,
    SettingsIndexKeyboardLayout,
};

const char* const on_off_text[2] = {
    "OFF",
    "ON",
};

// Vibration level options
const char* const vibration_text[4] = {
    "OFF",
    "Low",
    "Medium",
    "High",
};

// NDEF max length options
const char* const ndef_max_len_text[3] = {
    "250 chars",
    "500 chars",
    "1000 chars",
};

// Mode startup behavior options
const char* const mode_startup_text[6] = {
    "Remember",
    "NFC",
    "RFID",
    "NDEF",
    "NFC+RFID",
    "RFID+NFC",
};

// Output mode options
const char* const output_text[2] = {
    "USB",
    "BLE",
};

// Delimiter options - display names
const char* const delimiter_names[] = {
    "(empty)",
    ":",
    "-",
    "_",
    "space",
    ",",
    ";",
    "|",
};

// Delimiter options - actual values
const char* const delimiter_values[] = {
    "",    // empty
    ":",
    "-",
    "_",
    " ",   // space
    ",",
    ";",
    "|",
};

#define DELIMITER_OPTIONS_COUNT 8

// Keyboard layout storage (built-in + custom layouts from SD)
#define LAYOUT_BUILTIN_COUNT 2  // Default, NumPad
#define LAYOUT_MAX_CUSTOM 10    // Max custom layouts to show
static FuriString* layout_custom_names[LAYOUT_MAX_CUSTOM];
static FuriString* layout_custom_paths[LAYOUT_MAX_CUSTOM];
static size_t layout_custom_count = 0;
static size_t layout_total_count = LAYOUT_BUILTIN_COUNT;

// Built-in layout names
static const char* layout_builtin_names[LAYOUT_BUILTIN_COUNT] = {
    "Default (QWERTY)",
    "NumPad",
};

// Helper function to find delimiter index
static uint8_t get_delimiter_index(const char* delimiter) {
    for(uint8_t i = 0; i < DELIMITER_OPTIONS_COUNT; i++) {
        if(strcmp(delimiter, delimiter_values[i]) == 0) {
            return i;
        }
    }
    return 0; // Default to empty if not found
}

static void flipper_wedge_scene_settings_set_delimiter(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // Update delimiter in app
    strncpy(app->delimiter, delimiter_values[index], FLIPPER_WEDGE_DELIMITER_MAX_LEN - 1);
    app->delimiter[FLIPPER_WEDGE_DELIMITER_MAX_LEN - 1] = '\0';

    // Update display text
    variable_item_set_current_value_text(item, delimiter_names[index]);
}

static void flipper_wedge_scene_settings_set_append_enter(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);
    app->append_enter = (index == 1);
    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_mode_startup(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, mode_startup_text[index]);
    app->mode_startup_behavior = (FlipperWedgeModeStartup)index;
    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_vibration(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, vibration_text[index]);
    app->vibration_level = (FlipperWedgeVibration)index;
    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_ndef_max_len(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    FURI_LOG_I("Settings", "NDEF callback: index=%d, old app value=%d", index, app->ndef_max_len);
    variable_item_set_current_value_text(item, ndef_max_len_text[index]);
    app->ndef_max_len = (FlipperWedgeNdefMaxLen)index;
    FURI_LOG_I("Settings", "NDEF callback: new app value=%d, about to save", app->ndef_max_len);
    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_log_to_sd(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    FURI_LOG_I("Settings", "LogToSD callback: index=%d, old app value=%d", index, app->log_to_sd);
    variable_item_set_current_value_text(item, on_off_text[index]);
    app->log_to_sd = (index == 1);
    FURI_LOG_I("Settings", "LogToSD callback: new app value=%d, about to save", app->log_to_sd);
    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_keyboard_layout(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    FURI_LOG_I("Settings", "Layout callback: index=%d", index);

    // Get the layout name for display
    const char* layout_name;
    if(index < LAYOUT_BUILTIN_COUNT) {
        layout_name = layout_builtin_names[index];
    } else {
        size_t custom_index = index - LAYOUT_BUILTIN_COUNT;
        if(custom_index < layout_custom_count && layout_custom_names[custom_index]) {
            layout_name = furi_string_get_cstr(layout_custom_names[custom_index]);
        } else {
            layout_name = "???";
        }
    }
    variable_item_set_current_value_text(item, layout_name);

    // Apply the selected layout
    if(index == 0) {
        // Default (QWERTY)
        flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
    } else if(index == 1) {
        // NumPad
        flipper_wedge_keyboard_layout_set_numpad(app->keyboard_layout);
    } else {
        // Custom layout from file
        size_t custom_index = index - LAYOUT_BUILTIN_COUNT;
        if(custom_index < layout_custom_count && layout_custom_paths[custom_index]) {
            const char* path = furi_string_get_cstr(layout_custom_paths[custom_index]);
            if(!flipper_wedge_keyboard_layout_load(app->keyboard_layout, path)) {
                FURI_LOG_E("Settings", "Failed to load layout: %s", path);
                // Notify user of failure with error feedback
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_error);
                furi_record_close(RECORD_NOTIFICATION);
                // Fall back to default and update UI
                flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
                variable_item_set_current_value_index(item, 0);
                variable_item_set_current_value_text(item, "Default (QWERTY)");
            }
        }
    }

    flipper_wedge_save_settings(app);  // Save immediately to persist across app restarts
}

static void flipper_wedge_scene_settings_set_output(VariableItem* item) {
    FlipperWedge* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, output_text[index]);
    FlipperWedgeOutput new_output_mode = (FlipperWedgeOutput)index;

    // Handle output mode change with DEFERRED switching
    if(new_output_mode != app->output_mode) {
        FURI_LOG_I("Settings", "Requesting output mode switch: %s -> %s",
                   app->output_mode == FlipperWedgeOutputUsb ? "USB" : "BLE",
                   new_output_mode == FlipperWedgeOutputUsb ? "USB" : "BLE");

        // Set flag for tick callback to process (worker thread handles HID lifecycle)
        app->output_switch_pending = true;
        app->output_switch_target = new_output_mode;

        // IMPORTANT: Update output_mode immediately so it persists if user exits
        // before the async switch completes. The switch function checks the pending
        // flag, not the mode equality, so this is safe.
        app->output_mode = new_output_mode;
        flipper_wedge_save_settings(app);

        // Rebuild settings list to show/hide "Pair Bluetooth..." option
        scene_manager_handle_custom_event(app->scene_manager, SettingsIndexOutput);
    }
}

static void flipper_wedge_scene_settings_item_callback(void* context, uint32_t index) {
    FlipperWedge* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_wedge_scene_settings_on_enter(void* context) {
    FlipperWedge* app = context;
    VariableItem* item;

    // Keep display backlight on while in settings
    notification_message(app->notification, &sequence_display_backlight_enforce_on);

    // CRITICAL: Validate output_mode is within range (fix corrupted settings)
    if(app->output_mode >= FlipperWedgeOutputCount) {
        FURI_LOG_E("Settings", "Output mode %d out of range, forcing to USB", app->output_mode);
        app->output_mode = FlipperWedgeOutputUsb;
        flipper_wedge_save_settings(app);  // Save the fix
    }

    // Header with branding (non-interactive)
    item = variable_item_list_add(
        app->variable_item_list,
        "dangerousthings.com",
        0,
        NULL,
        app);

    // Output mode selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Output:",
        FlipperWedgeOutputCount,
        flipper_wedge_scene_settings_set_output,
        app);

    // Use target mode for display if switching is pending (prevents duplicate positions)
    FlipperWedgeOutput display_mode = app->output_switch_pending ?
        app->output_switch_target : app->output_mode;

    variable_item_set_current_value_index(item, display_mode);
    variable_item_set_current_value_text(item, output_text[display_mode]);

    // Pair Bluetooth... action (show in BLE mode or when switching to BLE)
    // Hide immediately when switching from BLE to USB for cleaner UX
    bool currently_ble = (app->output_mode == FlipperWedgeOutputBle);
    bool switching_to_ble = (app->output_switch_pending && app->output_switch_target == FlipperWedgeOutputBle);
    bool switching_from_ble = (app->output_switch_pending && app->output_mode == FlipperWedgeOutputBle);

    // Only show if in BLE mode or switching TO BLE (not FROM BLE)
    if((currently_ble || switching_to_ble) && !switching_from_ble) {
        const char* bt_status;

        // Determine status based on state
        if(switching_to_ble) {
            // Switching USB → BLE: Show "Initializing..." while starting BLE
            bt_status = "Initializing...";
        } else {
            // Normal BLE mode: Show connection status
            bool bt_connected = flipper_wedge_hid_is_bt_connected(flipper_wedge_get_hid(app));
            if(bt_connected) {
                bt_status = "Paired";
            } else {
                // Check if advertising (pairing mode)
                bool bt_advertising = furi_hal_bt_is_active();
                bt_status = bt_advertising ? "Pairing..." : "Not paired";
            }
        }

        item = variable_item_list_add(
            app->variable_item_list,
            "Pair Bluetooth...",
            1,
            NULL,  // No change callback
            app);
        variable_item_set_current_value_text(item, bt_status);
    }

    // Byte Delimiter selector
    uint8_t delimiter_index = get_delimiter_index(app->delimiter);
    item = variable_item_list_add(
        app->variable_item_list,
        "Byte Delimiter:",
        DELIMITER_OPTIONS_COUNT,
        flipper_wedge_scene_settings_set_delimiter,
        app);
    variable_item_set_current_value_index(item, delimiter_index);
    variable_item_set_current_value_text(item, delimiter_names[delimiter_index]);

    // Append Enter toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "Append Enter:",
        2,
        flipper_wedge_scene_settings_set_append_enter,
        app);
    variable_item_set_current_value_index(item, app->append_enter ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->append_enter ? 1 : 0]);

    // Mode startup behavior selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Start Mode:",
        FlipperWedgeModeStartupCount,
        flipper_wedge_scene_settings_set_mode_startup,
        app);
    variable_item_set_current_value_index(item, app->mode_startup_behavior);
    variable_item_set_current_value_text(item, mode_startup_text[app->mode_startup_behavior]);

    // Vibration level selector
    item = variable_item_list_add(
        app->variable_item_list,
        "Vibration:",
        FlipperWedgeVibrationCount,
        flipper_wedge_scene_settings_set_vibration,
        app);
    variable_item_set_current_value_index(item, app->vibration_level);
    variable_item_set_current_value_text(item, vibration_text[app->vibration_level]);

    // NDEF max length selector
    item = variable_item_list_add(
        app->variable_item_list,
        "NDEF Max Len:",
        FlipperWedgeNdefMaxLenCount,
        flipper_wedge_scene_settings_set_ndef_max_len,
        app);
    variable_item_set_current_value_index(item, app->ndef_max_len);
    variable_item_set_current_value_text(item, ndef_max_len_text[app->ndef_max_len]);

    // Log to SD toggle
    item = variable_item_list_add(
        app->variable_item_list,
        "Log to SD:",
        2,
        flipper_wedge_scene_settings_set_log_to_sd,
        app);
    variable_item_set_current_value_index(item, app->log_to_sd ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->log_to_sd ? 1 : 0]);

    // Keyboard Layout selector
    // First, free any previously allocated custom layout strings
    for(size_t i = 0; i < layout_custom_count; i++) {
        if(layout_custom_names[i]) {
            furi_string_free(layout_custom_names[i]);
            layout_custom_names[i] = NULL;
        }
        if(layout_custom_paths[i]) {
            furi_string_free(layout_custom_paths[i]);
            layout_custom_paths[i] = NULL;
        }
    }

    // Scan for custom layouts on SD card
    Storage* storage = furi_record_open(RECORD_STORAGE);
    layout_custom_count = flipper_wedge_keyboard_layout_list(
        storage,
        layout_custom_names,
        layout_custom_paths,
        LAYOUT_MAX_CUSTOM);
    furi_record_close(RECORD_STORAGE);

    layout_total_count = LAYOUT_BUILTIN_COUNT + layout_custom_count;

    // Determine current layout index
    uint8_t layout_index = 0;
    if(app->keyboard_layout) {
        if(app->keyboard_layout->type == FlipperWedgeLayoutDefault) {
            layout_index = 0;
        } else if(app->keyboard_layout->type == FlipperWedgeLayoutNumPad) {
            layout_index = 1;
        } else if(app->keyboard_layout->type == FlipperWedgeLayoutCustom) {
            // Find the matching custom layout by path
            for(size_t i = 0; i < layout_custom_count; i++) {
                if(layout_custom_paths[i] &&
                   strcmp(app->keyboard_layout->file_path, furi_string_get_cstr(layout_custom_paths[i])) == 0) {
                    layout_index = LAYOUT_BUILTIN_COUNT + i;
                    break;
                }
            }
        }
    }

    // Get current layout name for display
    const char* current_layout_name;
    if(layout_index < LAYOUT_BUILTIN_COUNT) {
        current_layout_name = layout_builtin_names[layout_index];
    } else {
        size_t custom_idx = layout_index - LAYOUT_BUILTIN_COUNT;
        if(custom_idx < layout_custom_count && layout_custom_names[custom_idx]) {
            current_layout_name = furi_string_get_cstr(layout_custom_names[custom_idx]);
        } else {
            current_layout_name = "???";
        }
    }

    item = variable_item_list_add(
        app->variable_item_list,
        "KB Layout:",
        layout_total_count,
        flipper_wedge_scene_settings_set_keyboard_layout,
        app);
    variable_item_set_current_value_index(item, layout_index);
    variable_item_set_current_value_text(item, current_layout_name);

    // Set callback for when user clicks on an item
    variable_item_list_set_enter_callback(
        app->variable_item_list,
        flipper_wedge_scene_settings_item_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperWedgeViewIdSettings);
}

bool flipper_wedge_scene_settings_on_event(void* context, SceneManagerEvent event) {
    FlipperWedge* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SettingsIndexOutput) {
            // Output mode changed - rebuild list to show/hide Pair BT option
            variable_item_list_reset(app->variable_item_list);
            flipper_wedge_scene_settings_on_enter(context);
            consumed = true;
        } else if(event.event == SettingsIndexBtPair) {
            // User clicked "Pair Bluetooth..." - navigate to pairing scene
            scene_manager_next_scene(app->scene_manager, FlipperWedgeSceneBtPair);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Periodically check if BT connection status changed or if switching modes
        static bool last_bt_connected = false;
        static bool last_bt_advertising = false;
        static bool last_switching_pending = false;
        static FlipperWedgeOutput last_output_mode = FlipperWedgeOutputUsb;
        static uint8_t tick_counter = 0;

        // Check more frequently during/after transitions (every 2 ticks = 200ms)
        // Check less frequently when stable (every 10 ticks = 1 second)
        bool was_switching = last_switching_pending;
        bool in_transition = app->output_switch_pending || was_switching;
        uint8_t check_interval = in_transition ? 2 : 10;

        tick_counter++;
        if(tick_counter >= check_interval) {
            tick_counter = 0;

            bool currently_ble = (app->output_mode == FlipperWedgeOutputBle);
            bool switching = app->output_switch_pending;

            // Check if we need to rebuild the list
            bool needs_rebuild = false;

            // Always rebuild if switching state changed
            if(switching != last_switching_pending) {
                needs_rebuild = true;
                last_switching_pending = switching;
                FURI_LOG_I("Settings", "Switching state changed: %d -> %d", !switching, switching);
            }

            // Rebuild if output mode actually changed (switch completed)
            if(app->output_mode != last_output_mode) {
                needs_rebuild = true;
                last_output_mode = app->output_mode;
                FURI_LOG_I("Settings", "Output mode changed: %d -> %d", last_output_mode, app->output_mode);
            }

            // Check BT status changes when in BLE mode or switching
            if(currently_ble || switching) {
                bool bt_connected = flipper_wedge_hid_is_bt_connected(flipper_wedge_get_hid(app));
                bool bt_advertising = furi_hal_bt_is_active();

                if(bt_connected != last_bt_connected || bt_advertising != last_bt_advertising) {
                    needs_rebuild = true;
                    last_bt_connected = bt_connected;
                    last_bt_advertising = bt_advertising;
                }
            }

            // Rebuild if needed
            if(needs_rebuild) {
                FURI_LOG_I("Settings", "Status changed, rebuilding list");
                variable_item_list_reset(app->variable_item_list);
                flipper_wedge_scene_settings_on_enter(context);
            }
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        // Save settings when leaving
        flipper_wedge_save_settings(app);
    }

    return consumed;
}

void flipper_wedge_scene_settings_on_exit(void* context) {
    FlipperWedge* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);

    // Free custom layout strings
    for(size_t i = 0; i < layout_custom_count; i++) {
        if(layout_custom_names[i]) {
            furi_string_free(layout_custom_names[i]);
            layout_custom_names[i] = NULL;
        }
        if(layout_custom_paths[i]) {
            furi_string_free(layout_custom_paths[i]);
            layout_custom_paths[i] = NULL;
        }
    }
    layout_custom_count = 0;
    layout_total_count = LAYOUT_BUILTIN_COUNT;

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}
