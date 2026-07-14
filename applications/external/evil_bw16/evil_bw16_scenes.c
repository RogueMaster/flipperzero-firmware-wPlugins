#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "evil_bw16.h"

// Configuration menu items
enum EvilBw16ConfigMenuIndex {
    EvilBw16ConfigMenuIndexCycleDelay = 0,
    EvilBw16ConfigMenuIndexScanTime,
    EvilBw16ConfigMenuIndexFramesPerAP,
    EvilBw16ConfigMenuIndexStartChannel,
    EvilBw16ConfigMenuIndexLedEnabled,
    EvilBw16ConfigMenuIndexDebugMode,
    EvilBw16ConfigMenuIndexGpioPins,
    EvilBw16ConfigMenuIndexSendToDevice,
};

// Debug logging functions
void debug_write_to_sd(const char* data) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    // Ensure debug directory exists
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));

    // Open debug log file in append mode
    if(storage_file_open(
           file, EXT_PATH("apps_data/evil_bw16_debug.txt"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        // Write timestamp and data
        char timestamp[32];
        uint32_t tick = furi_get_tick();
        snprintf(timestamp, sizeof(timestamp), "[%lu] ", tick);

        storage_file_write(file, timestamp, strlen(timestamp));
        storage_file_write(file, data, strlen(data));
        storage_file_write(file, "\n", 1);
        storage_file_sync(file);

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void debug_clear_log() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, EXT_PATH("apps_data/evil_bw16_debug.txt"));
    furi_record_close(RECORD_STORAGE);
}

// Submenu callback functions
static void evil_bw16_submenu_callback_main_menu(void* context, uint32_t index) {
    EvilBw16App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void evil_bw16_submenu_callback_attacks(void* context, uint32_t index) {
    EvilBw16App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void evil_bw16_submenu_callback_sniffer(void* context, uint32_t index) {
    EvilBw16App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Text input callback for UART terminal
static void evil_bw16_text_input_callback_uart(void* context) {
    EvilBw16App* app = context;

    // Send the custom command (already logs TX automatically)
    if(strlen(app->text_input_store) > 0) {
        evil_bw16_send_command(app, app->text_input_store);
    }

    // Switch to terminal display mode
    scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
    scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
}

// Text input callback for configuration
static void evil_bw16_config_input_callback(void* context) {
    EvilBw16App* app = context;

    if(strlen(app->text_input_store) > 0) {
        uint32_t value = atoi(app->text_input_store);

        // Update the configuration based on which item was being edited
        switch(app->selected_network_index) {
        case EvilBw16ConfigMenuIndexCycleDelay:
            if(value >= 100 && value <= 60000) { // 100ms to 60s
                app->config.cycle_delay = value;
            }
            break;

        case EvilBw16ConfigMenuIndexScanTime:
            if(value >= 1000 && value <= 30000) { // 1s to 30s
                app->config.scan_time = value;
            }
            break;

        case EvilBw16ConfigMenuIndexFramesPerAP:
            if(value >= 1 && value <= 20) { // 1 to 20 frames
                app->config.num_frames = value;
            }
            break;

        case EvilBw16ConfigMenuIndexStartChannel:
            if(value >= 1 && value <= 14) { // WiFi channels 1-14
                app->config.start_channel = (uint8_t)value;
            }
            break;
        }
    }

    // Go back to config menu
    scene_manager_next_scene(app->scene_manager, EvilBw16SceneConfig);
}

// Scene: Start
void evil_bw16_scene_on_enter_start(void* context) {
    EvilBw16App* app = context;

    // Skip startup screen - go directly to main menu
    scene_manager_next_scene(app->scene_manager, EvilBw16SceneMainMenu);
}

bool evil_bw16_scene_on_event_start(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EvilBw16EventExit) {
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_start(void* context) {
    UNUSED(context);
}

// Scene: Main Menu
enum EvilBw16MainMenuIndex {
    EvilBw16MainMenuIndexScanner,
    EvilBw16MainMenuIndexAttacks,
    EvilBw16MainMenuIndexSniffer,
    EvilBw16MainMenuIndexConfig,
    EvilBw16MainMenuIndexDeviceInfo,
    EvilBw16MainMenuIndexUartTerminal,
};

void evil_bw16_scene_on_enter_main_menu(void* context) {
    EvilBw16App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Evil BW16 Controller");

    submenu_add_item(
        app->submenu,
        "WiFi Scanner",
        EvilBw16MainMenuIndexScanner,
        evil_bw16_submenu_callback_main_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Attack Mode",
        EvilBw16MainMenuIndexAttacks,
        evil_bw16_submenu_callback_main_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Set Targets",
        EvilBw16MainMenuIndexSniffer,
        evil_bw16_submenu_callback_main_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Configuration",
        EvilBw16MainMenuIndexConfig,
        evil_bw16_submenu_callback_main_menu,
        app);
    submenu_add_item(
        app->submenu,
        "Help",
        EvilBw16MainMenuIndexDeviceInfo,
        evil_bw16_submenu_callback_main_menu,
        app);
    submenu_add_item(
        app->submenu,
        "UART Terminal",
        EvilBw16MainMenuIndexUartTerminal,
        evil_bw16_submenu_callback_main_menu,
        app);

    submenu_set_selected_item(app->submenu, app->selected_menu_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewMainMenu);
}

bool evil_bw16_scene_on_event_main_menu(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Exit the app when back is pressed from main menu
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        app->selected_menu_index = event.event;

        switch(event.event) {
        case EvilBw16MainMenuIndexScanner:
            // Send scan command and open terminal to see results immediately
            evil_bw16_send_command(app, "scan");
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;
        case EvilBw16MainMenuIndexAttacks:
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneAttacks);
            return true;
        case EvilBw16MainMenuIndexSniffer:
            // Go to target selection scene (repurposed sniffer scene)
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneSniffer);
            return true;
        case EvilBw16MainMenuIndexConfig:
            // Go to proper configuration scene
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneConfig);
            return true;
        case EvilBw16MainMenuIndexDeviceInfo:
            // Go to help scene
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneDeviceInfo);
            return true;
        case EvilBw16MainMenuIndexUartTerminal:
            // Go to text input first for custom commands
            strncpy(app->text_input_store, "", EVIL_BW16_TEXT_INPUT_STORE_SIZE - 1);
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 0);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_main_menu(void* context) {
    EvilBw16App* app = context;
    submenu_reset(app->submenu);
}

// Scene: Scanner
void evil_bw16_scene_on_enter_scanner(void* context) {
    EvilBw16App* app = context;

    // Show loading screen for a few seconds, then show fake results
    evil_bw16_show_loading(app, "Scanning WiFi networks...");

    // Clear previous scan results and selections completely
    app->network_count = 0;
    app->scan_in_progress = true;

    // Explicitly clear all networks and reset selection state
    for(int i = 0; i < EVIL_BW16_MAX_NETWORKS; i++) {
        memset(&app->networks[i], 0, sizeof(EvilBw16Network));
        app->networks[i].selected = false;
        app->networks[i].index = i; // Internal array index
        app->networks[i].device_index = i; // Will be set properly by UART parser
    }

    // Clear attack state targets since networks are changing
    app->attack_state.num_targets = 0;
    memset(app->attack_state.target_indices, 0, sizeof(app->attack_state.target_indices));

    // Clear debug log at start of new scan
    debug_clear_log();
    debug_write_to_sd("=== NEW SCAN STARTED ===");

    FURI_LOG_I(
        "EvilBw16", "Scanner: Cleared %d networks and all selections", EVIL_BW16_MAX_NETWORKS);

    // Send scan command - UART worker will handle all parsing
    evil_bw16_send_scan_command(app);
}

bool evil_bw16_scene_on_event_scanner(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    // Handle scan completion event from UART worker
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EvilBw16EventScanComplete) {
            FURI_LOG_I(
                "EvilBw16", "Scan complete event received with %d networks", app->network_count);
            app->scan_in_progress = false;
            evil_bw16_hide_loading(app);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneScannerResults);
            return true;
        }
    }

    // Timeout after 15 seconds in case scan gets stuck
    static uint32_t scan_start_time = 0;
    if(scan_start_time == 0) {
        scan_start_time = furi_get_tick();
    }

    if(furi_get_tick() - scan_start_time > 15000) {
        FURI_LOG_I("EvilBw16", "Scan timeout reached with %d networks found", app->network_count);

        if(app->network_count == 0) {
            // No networks found after timeout - add diagnostic info
            strcpy(app->networks[0].ssid, "Scan Timeout");
            strcpy(app->networks[0].bssid, "No UART Data");
            app->networks[0].channel = 1;
            app->networks[0].rssi = -99;
            app->networks[0].band = EvilBw16Band24GHz;
            app->networks[0].index = 0;
            app->networks[0].selected = false;

            strcpy(app->networks[1].ssid, "BW16 Not Responding");
            const char* pins = (app->config.gpio_pins == EvilBw16GpioPins13_14) ? "Pin 13/14" :
                                                                                  "Pin 15/16";
            strcpy(app->networks[1].bssid, pins);
            app->networks[1].channel = 36;
            app->networks[1].rssi = -99;
            app->networks[1].band = EvilBw16Band5GHz;
            app->networks[1].index = 1;
            app->networks[1].selected = false;

            app->network_count = 2;
        }

        app->scan_in_progress = false;
        evil_bw16_hide_loading(app);
        scene_manager_next_scene(app->scene_manager, EvilBw16SceneScannerResults);
        scan_start_time = 0;
        return true;
    }

    return false;
}

void evil_bw16_scene_on_exit_scanner(void* context) {
    EvilBw16App* app = context;
    evil_bw16_hide_loading(app);
}

// Scene: Scanner Results
void evil_bw16_scene_on_enter_scanner_results(void* context) {
    EvilBw16App* app = context;

    furi_string_reset(app->text_box_string);

    if(app->network_count == 0) {
        furi_string_printf(
            app->text_box_string, "No networks found.\n\nPress BACK to return to menu.");
    } else {
        furi_string_printf(app->text_box_string, "Found %d networks:\n\n", app->network_count);

        for(uint8_t i = 0; i < app->network_count; i++) {
            char freq_str[8];
            if(app->networks[i].channel >= 36) {
                strcpy(freq_str, "5GHz");
            } else {
                strcpy(freq_str, "2.4GHz");
            }

            furi_string_cat_printf(
                app->text_box_string,
                "[%02d] %s\n"
                "     %s\n"
                "     Ch:%d  %ddBm  %s\n\n",
                i + 1, // Show 1-based indexing consistently
                app->networks[i].ssid,
                app->networks[i].bssid,
                app->networks[i].channel,
                app->networks[i].rssi,
                freq_str);
        }

        furi_string_cat_printf(
            app->text_box_string, "Press OK to select targets\nPress BACK to return to menu");
    }

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_string));
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextBox);
}

bool evil_bw16_scene_on_event_scanner_results(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0) { // OK pressed
            // Go to attack configuration
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneAttackConfig);
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_scanner_results(void* context) {
    EvilBw16App* app = context;
    text_box_reset(app->text_box);
}

// Scene: Attacks
enum EvilBw16AttacksMenuIndex {
    EvilBw16AttacksMenuIndexDeauth,
    EvilBw16AttacksMenuIndexDisassoc,
    EvilBw16AttacksMenuIndexRandom,
    EvilBw16AttacksMenuIndexStop,
};

void evil_bw16_scene_on_enter_attacks(void* context) {
    EvilBw16App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Attack Mode");

    submenu_add_item(
        app->submenu,
        "Deauth Attack",
        EvilBw16AttacksMenuIndexDeauth,
        evil_bw16_submenu_callback_attacks,
        app);
    submenu_add_item(
        app->submenu,
        "Disassoc Attack",
        EvilBw16AttacksMenuIndexDisassoc,
        evil_bw16_submenu_callback_attacks,
        app);
    submenu_add_item(
        app->submenu,
        "Random Attack",
        EvilBw16AttacksMenuIndexRandom,
        evil_bw16_submenu_callback_attacks,
        app);
    submenu_add_item(
        app->submenu,
        "Stop Attack",
        EvilBw16AttacksMenuIndexStop,
        evil_bw16_submenu_callback_attacks,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewMainMenu);
}

bool evil_bw16_scene_on_event_attacks(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EvilBw16AttacksMenuIndexDeauth:
            evil_bw16_send_attack_start(app);
            app->attack_state.active = true;
            app->attack_state.mode = EvilBw16AttackModeDeauth;
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;

        case EvilBw16AttacksMenuIndexDisassoc:
            evil_bw16_send_command(app, "disassoc");
            app->attack_state.active = true;
            app->attack_state.mode = EvilBw16AttackModeDisassoc;
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;

        case EvilBw16AttacksMenuIndexRandom:
            evil_bw16_send_command(app, "random_attack");
            app->attack_state.mode = EvilBw16AttackModeRandom;
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;

        case EvilBw16AttacksMenuIndexStop:
            evil_bw16_send_attack_stop(app);
            app->attack_state.active = false;
            scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
            scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_attacks(void* context) {
    EvilBw16App* app = context;
    submenu_reset(app->submenu);
}

// Scene: Attack Config
void evil_bw16_scene_on_enter_attack_config(void* context) {
    EvilBw16App* app = context;

    furi_string_reset(app->text_box_string);
    furi_string_printf(app->text_box_string, "Attack Configuration\n\n");

    // Show current targets
    if(app->attack_state.num_targets > 0) {
        furi_string_cat_printf(app->text_box_string, "Current targets:\n");
        for(uint8_t i = 0; i < app->attack_state.num_targets; i++) {
            uint8_t idx = app->attack_state.target_indices[i];
            if(idx < app->network_count) {
                furi_string_cat_printf(
                    app->text_box_string, "[%02d] %s\n", idx + 1, app->networks[idx].ssid);
            }
        }
    } else {
        furi_string_cat_printf(app->text_box_string, "No targets selected.\n");
    }

    furi_string_cat_printf(app->text_box_string, "\nPress OK to configure targets");

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_string));
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextBox);
}

bool evil_bw16_scene_on_event_attack_config(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void evil_bw16_scene_on_exit_attack_config(void* context) {
    EvilBw16App* app = context;
    text_box_reset(app->text_box);
}

// Scene: Sniffer
enum EvilBw16TargetMenuIndex {
    EvilBw16TargetMenuIndexClearAll = 100,
    EvilBw16TargetMenuIndexSelectAll = 101,
    EvilBw16TargetMenuIndexConfirm = 102,
};

void evil_bw16_scene_on_enter_sniffer(void* context) {
    EvilBw16App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Set Attack Targets");

    if(app->network_count == 0) {
        submenu_add_item(app->submenu, "No Networks Found", 0, NULL, app);
        submenu_add_item(app->submenu, "Run WiFi Scan First", 0, NULL, app);
    } else {
        // Show all available networks with improved formatting
        for(uint8_t i = 0; i < app->network_count; i++) {
            char menu_text[100];
            const char* status = app->networks[i].selected ? "✓" : "○";
            char band_str[8];
            strcpy(band_str, (app->networks[i].band == EvilBw16Band5GHz) ? "5G" : "2.4G");

            // Truncate long SSIDs for better display
            char ssid_display[32];
            if(strlen(app->networks[i].ssid) > 28) {
                strncpy(ssid_display, app->networks[i].ssid, 25);
                ssid_display[25] = '.';
                ssid_display[26] = '.';
                ssid_display[27] = '.';
                ssid_display[28] = '\0';
            } else {
                strcpy(ssid_display, app->networks[i].ssid);
            }

            snprintf(
                menu_text,
                sizeof(menu_text),
                "%s [%02d] %s (%s)",
                status,
                i + 1, // Show 1-based index to match what we send to device
                ssid_display,
                band_str);

            submenu_add_item(app->submenu, menu_text, i, evil_bw16_submenu_callback_sniffer, app);

            FURI_LOG_D(
                "EvilBw16",
                "Menu[%d]: '%s' -> device_idx=%d",
                i,
                ssid_display,
                app->networks[i].device_index);
        }

        // Count selected targets for status display
        int selected_targets = 0;
        for(uint8_t i = 0; i < app->network_count; i++) {
            if(app->networks[i].selected) selected_targets++;
        }

        // Add control options with status
        submenu_add_item(
            app->submenu,
            "Clear All Targets",
            EvilBw16TargetMenuIndexClearAll,
            evil_bw16_submenu_callback_sniffer,
            app);
        submenu_add_item(
            app->submenu,
            "Select All Targets",
            EvilBw16TargetMenuIndexSelectAll,
            evil_bw16_submenu_callback_sniffer,
            app);

        char confirm_text[64];
        snprintf(
            confirm_text, sizeof(confirm_text), "Confirm Targets (%d selected)", selected_targets);
        submenu_add_item(
            app->submenu,
            confirm_text,
            EvilBw16TargetMenuIndexConfirm,
            evil_bw16_submenu_callback_sniffer,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewMainMenu);
}

bool evil_bw16_scene_on_event_sniffer(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t selection = event.event;

        // Add debugging for selection issues
        FURI_LOG_I(
            "EvilBw16",
            "Target selection: event=%lu, network_count=%d",
            selection,
            app->network_count);

        if(selection < app->network_count) {
            // Validate the network index before toggling
            if(app->networks[selection].ssid[0] != '\0') { // Ensure network exists
                bool was_selected = app->networks[selection].selected;
                app->networks[selection].selected = !was_selected;

                FURI_LOG_I(
                    "EvilBw16",
                    "TOGGLE: User clicked menu[%lu] '%s' (device_idx=%d): %s -> %s",
                    selection,
                    app->networks[selection].ssid,
                    app->networks[selection].device_index,
                    was_selected ? "selected" : "unselected",
                    app->networks[selection].selected ? "selected" : "unselected");

                // Refresh the scene by re-entering it (without adding to stack)
                evil_bw16_scene_on_exit_sniffer(app);
                evil_bw16_scene_on_enter_sniffer(app);
                return true;
            } else {
                FURI_LOG_W(
                    "EvilBw16", "Invalid network selection: index %lu has empty SSID", selection);
            }
        }

        switch(selection) {
        case EvilBw16TargetMenuIndexClearAll:
            // Clear all selections
            for(uint8_t i = 0; i < app->network_count; i++) {
                app->networks[i].selected = false;
            }
            // Refresh scene by re-entering it (without adding to stack)
            evil_bw16_scene_on_exit_sniffer(app);
            evil_bw16_scene_on_enter_sniffer(app);
            return true;

        case EvilBw16TargetMenuIndexSelectAll:
            // Select all networks
            for(uint8_t i = 0; i < app->network_count; i++) {
                app->networks[i].selected = true;
            }
            // Refresh scene by re-entering it (without adding to stack)
            evil_bw16_scene_on_exit_sniffer(app);
            evil_bw16_scene_on_enter_sniffer(app);
            return true;

        case EvilBw16TargetMenuIndexConfirm:
            // Build target list string and send to Arduino using device indices
            FuriString* target_string = furi_string_alloc();
            bool first = true;
            int selected_count = 0;

            for(uint8_t i = 0; i < app->network_count; i++) {
                if(app->networks[i].selected) {
                    if(!first) {
                        furi_string_cat_str(target_string, ",");
                    }
                    // FIX: BW16 device uses 1-based indexing, so add 1 to our 0-based menu index
                    furi_string_cat_printf(target_string, "%d", i + 1);
                    first = false;
                    selected_count++;

                    FURI_LOG_I(
                        "EvilBw16",
                        "CONFIRM: User selected menu[%d]: '%s' -> sending 1-based index=%d",
                        i,
                        app->networks[i].ssid,
                        i + 1);
                }
            }

            if(furi_string_size(target_string) > 0) {
                // Send target command to Arduino
                FuriString* set_target_cmd = furi_string_alloc();
                furi_string_printf(
                    set_target_cmd, "set target %s", furi_string_get_cstr(target_string));

                FURI_LOG_I(
                    "EvilBw16",
                    "Sending target command: '%s' (%d targets)",
                    furi_string_get_cstr(set_target_cmd),
                    selected_count);

                evil_bw16_send_command(app, furi_string_get_cstr(set_target_cmd));
                furi_string_free(set_target_cmd);

                // Show confirmation and go to terminal
                scene_manager_set_scene_state(app->scene_manager, EvilBw16SceneUartTerminal, 1);
                scene_manager_next_scene(app->scene_manager, EvilBw16SceneUartTerminal);
            } else {
                // No targets selected - show popup
                evil_bw16_show_popup(
                    app, "No Targets", "Please select at least one target network");
            }

            furi_string_free(target_string);
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_sniffer(void* context) {
    EvilBw16App* app = context;
    submenu_reset(app->submenu);
}

// Scene: Sniffer Results
void evil_bw16_scene_on_enter_sniffer_results(void* context) {
    EvilBw16App* app = context;

    evil_bw16_clear_log(app);
    furi_string_printf(app->log_string, "Packet Sniffer Active\nMode: ");

    switch(app->sniffer_state.mode) {
    case EvilBw16SniffModeAll:
        furi_string_cat_printf(app->log_string, "ALL");
        break;
    case EvilBw16SniffModeBeacon:
        furi_string_cat_printf(app->log_string, "BEACON");
        break;
    case EvilBw16SniffModeProbe:
        furi_string_cat_printf(app->log_string, "PROBE");
        break;
    case EvilBw16SniffModeDeauth:
        furi_string_cat_printf(app->log_string, "DEAUTH");
        break;
    case EvilBw16SniffModeEapol:
        furi_string_cat_printf(app->log_string, "EAPOL");
        break;
    case EvilBw16SniffModePwnagotchi:
        furi_string_cat_printf(app->log_string, "PWNAGOTCHI");
        break;
    }

    furi_string_cat_printf(
        app->log_string,
        "\nChannel: %d\nHopping: %s\n\n",
        app->sniffer_state.channel,
        app->sniffer_state.hopping ? "ON" : "OFF");

    text_box_set_text(app->text_box, furi_string_get_cstr(app->log_string));
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextBox);
}

bool evil_bw16_scene_on_event_sniffer_results(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;
    UNUSED(event);

    // For now, just show static content without reading UART
    static uint32_t last_update = 0;
    if(furi_get_tick() - last_update > 2000) { // Update every 2 seconds
        app->sniffer_state.packet_count++;
        evil_bw16_append_log(app, "Packet capture simulation running...");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->log_string));
        text_box_set_focus(app->text_box, TextBoxFocusEnd);
        last_update = furi_get_tick();
    }

    return false;
}

void evil_bw16_scene_on_exit_sniffer_results(void* context) {
    EvilBw16App* app = context;
    text_box_reset(app->text_box);
}

// Scene: Config
void evil_bw16_scene_on_enter_config(void* context) {
    EvilBw16App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Configuration Settings");

    char temp_str[64];

    snprintf(temp_str, sizeof(temp_str), "Cycle Delay: %lu ms", app->config.cycle_delay);
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexCycleDelay,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(temp_str, sizeof(temp_str), "Scan Time: %lu ms", app->config.scan_time);
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexScanTime,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(temp_str, sizeof(temp_str), "Frames per AP: %lu", app->config.num_frames);
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexFramesPerAP,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(temp_str, sizeof(temp_str), "Start Channel: %d", app->config.start_channel);
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexStartChannel,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(
        temp_str, sizeof(temp_str), "LED Enabled: %s", app->config.led_enabled ? "Yes" : "No");
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexLedEnabled,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(temp_str, sizeof(temp_str), "Debug Mode: %s", app->config.debug_mode ? "Yes" : "No");
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexDebugMode,
        evil_bw16_submenu_callback_attacks,
        app);

    snprintf(
        temp_str,
        sizeof(temp_str),
        "GPIO Pins: %s",
        app->config.gpio_pins == EvilBw16GpioPins13_14 ? "13/14" : "15/16");
    submenu_add_item(
        app->submenu,
        temp_str,
        EvilBw16ConfigMenuIndexGpioPins,
        evil_bw16_submenu_callback_attacks,
        app);

    submenu_add_item(
        app->submenu,
        "Send Config to Device",
        EvilBw16ConfigMenuIndexSendToDevice,
        evil_bw16_submenu_callback_attacks,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewMainMenu);
}

bool evil_bw16_scene_on_event_config(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EvilBw16ConfigMenuIndexCycleDelay:
            // Set up text input for cycle delay
            strncpy(app->text_input_store, "2000", EVIL_BW16_TEXT_INPUT_STORE_SIZE - 1);
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Cycle Delay (ms):");
            text_input_set_result_callback(
                app->text_input,
                evil_bw16_config_input_callback,
                app,
                app->text_input_store,
                EVIL_BW16_TEXT_INPUT_STORE_SIZE,
                true);
            app->selected_network_index =
                EvilBw16ConfigMenuIndexCycleDelay; // Store which config item we're editing
            view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextInput);
            return true;

        case EvilBw16ConfigMenuIndexScanTime:
            strncpy(app->text_input_store, "5000", EVIL_BW16_TEXT_INPUT_STORE_SIZE - 1);
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Scan Time (ms):");
            text_input_set_result_callback(
                app->text_input,
                evil_bw16_config_input_callback,
                app,
                app->text_input_store,
                EVIL_BW16_TEXT_INPUT_STORE_SIZE,
                true);
            app->selected_network_index = EvilBw16ConfigMenuIndexScanTime;
            view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextInput);
            return true;

        case EvilBw16ConfigMenuIndexFramesPerAP:
            strncpy(app->text_input_store, "3", EVIL_BW16_TEXT_INPUT_STORE_SIZE - 1);
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Frames per AP:");
            text_input_set_result_callback(
                app->text_input,
                evil_bw16_config_input_callback,
                app,
                app->text_input_store,
                EVIL_BW16_TEXT_INPUT_STORE_SIZE,
                true);
            app->selected_network_index = EvilBw16ConfigMenuIndexFramesPerAP;
            view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextInput);
            return true;

        case EvilBw16ConfigMenuIndexStartChannel:
            strncpy(app->text_input_store, "1", EVIL_BW16_TEXT_INPUT_STORE_SIZE - 1);
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Start Channel (1-14):");
            text_input_set_result_callback(
                app->text_input,
                evil_bw16_config_input_callback,
                app,
                app->text_input_store,
                EVIL_BW16_TEXT_INPUT_STORE_SIZE,
                true);
            app->selected_network_index = EvilBw16ConfigMenuIndexStartChannel;
            view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextInput);
            return true;

        case EvilBw16ConfigMenuIndexLedEnabled:
            // Toggle LED setting
            app->config.led_enabled = !app->config.led_enabled;
            // Refresh menu to show updated value
            evil_bw16_scene_on_exit_config(app);
            evil_bw16_scene_on_enter_config(app);
            return true;

        case EvilBw16ConfigMenuIndexDebugMode:
            // Toggle debug mode
            app->config.debug_mode = !app->config.debug_mode;
            // Refresh menu to show updated value
            evil_bw16_scene_on_exit_config(app);
            evil_bw16_scene_on_enter_config(app);
            return true;

        case EvilBw16ConfigMenuIndexGpioPins:
            // Toggle GPIO pins setting
            app->config.gpio_pins = (app->config.gpio_pins == EvilBw16GpioPins13_14) ?
                                        EvilBw16GpioPins15_16 :
                                        EvilBw16GpioPins13_14;

            // Show loading popup while restarting UART
            evil_bw16_show_popup(app, "Switching GPIO", "Restarting UART...");

            // Restart UART worker with new GPIO configuration
            evil_bw16_uart_restart(app);

            // Show success popup
            const char* pin_text = (app->config.gpio_pins == EvilBw16GpioPins13_14) ? "13/14" :
                                                                                      "15/16";
            char success_msg[64];
            snprintf(success_msg, sizeof(success_msg), "Switched to GPIO %s", pin_text);
            evil_bw16_show_popup(app, "GPIO Changed", success_msg);

            // Refresh menu to show updated value
            evil_bw16_scene_on_exit_config(app);
            evil_bw16_scene_on_enter_config(app);
            return true;

        case EvilBw16ConfigMenuIndexSendToDevice:
            // Send all config settings to BW16
            evil_bw16_send_config_to_device(app);
            evil_bw16_show_popup(app, "Config Sent", "Configuration sent to BW16 device");
            return true;
        }
    }

    return false;
}

void evil_bw16_scene_on_exit_config(void* context) {
    EvilBw16App* app = context;
    submenu_reset(app->submenu);
}

// Scene: Help
void evil_bw16_scene_on_enter_device_info(void* context) {
    EvilBw16App* app = context;

    furi_string_reset(app->text_box_string);
    furi_string_printf(app->text_box_string, "Evil BW16 Controller Help\n\n");

    furi_string_cat_printf(app->text_box_string, "=== HARDWARE SETUP ===\n\n");
    furi_string_cat_printf(app->text_box_string, "BW16 Module Connections:\n");
    furi_string_cat_printf(app->text_box_string, "Pin 1  (5V)  -> BW16 5V\n");
    if(app->config.gpio_pins == EvilBw16GpioPins13_14) {
        furi_string_cat_printf(app->text_box_string, "Pin 13 (TX)  -> BW16 PB1 (RX)\n");
        furi_string_cat_printf(app->text_box_string, "Pin 14 (RX)  -> BW16 PB2 (TX)\n");
    } else {
        furi_string_cat_printf(app->text_box_string, "Pin 15 (TX)  -> BW16 PB1 (RX)\n");
        furi_string_cat_printf(app->text_box_string, "Pin 16 (RX)  -> BW16 PB2 (TX)\n");
    }
    furi_string_cat_printf(app->text_box_string, "Pin 18 (GND) -> BW16 GND\n\n");

    furi_string_cat_printf(app->text_box_string, "=== HOW TO USE ===\n\n");
    furi_string_cat_printf(app->text_box_string, "1. WiFi Scanner - Scan for networks\n");
    furi_string_cat_printf(app->text_box_string, "2. Set Targets - Choose networks to attack\n");
    furi_string_cat_printf(app->text_box_string, "3. Attack Mode - Launch deauth attacks\n");
    furi_string_cat_printf(app->text_box_string, "4. Configuration - Adjust settings\n");
    furi_string_cat_printf(app->text_box_string, "5. UART Terminal - Direct communication\n\n");

    furi_string_cat_printf(app->text_box_string, "=== FEATURES ===\n\n");
    furi_string_cat_printf(app->text_box_string, "- Dual-band WiFi (2.4GHz + 5GHz)\n");
    furi_string_cat_printf(app->text_box_string, "- Deauth/Disassoc attacks\n");
    furi_string_cat_printf(app->text_box_string, "- Real-time packet monitoring\n");
    furi_string_cat_printf(app->text_box_string, "- Multi-target selection\n\n");

    furi_string_cat_printf(app->text_box_string, "App developed by dag nazty\n");
    furi_string_cat_printf(app->text_box_string, "BW16 firmware by 7h30th3r0n3");

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_string));
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextBox);
}

bool evil_bw16_scene_on_event_device_info(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void evil_bw16_scene_on_exit_device_info(void* context) {
    EvilBw16App* app = context;
    text_box_reset(app->text_box);
}

// Scene: UART Terminal
void evil_bw16_scene_on_enter_uart_terminal(void* context) {
    EvilBw16App* app = context;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, EvilBw16SceneUartTerminal);

    if(state == 0) {
        // Text input mode - let user type custom command
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Enter UART Command:");
        text_input_set_result_callback(
            app->text_input,
            evil_bw16_text_input_callback_uart,
            app,
            app->text_input_store,
            EVIL_BW16_TEXT_INPUT_STORE_SIZE,
            true);
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextInput);
    } else {
        // Terminal display mode - show UART communication
        // Initialize UART log with header
        furi_string_reset(app->uart_log_string);
        furi_string_printf(app->uart_log_string, "=== UART TERMINAL ===\n");
        const char* gpio_pins = (app->config.gpio_pins == EvilBw16GpioPins13_14) ? "13↔14" :
                                                                                   "15↔16";
        furi_string_cat_printf(app->uart_log_string, "115200 baud, GPIO %s\n", gpio_pins);
        furi_string_cat_printf(app->uart_log_string, "BW16 PB2←TX  PB1→RX\n");
        furi_string_cat_printf(app->uart_log_string, "Status: LISTENING...\n\n");

        // Immediately start displaying any existing log content
        if(furi_string_size(app->log_string) > 0) {
            furi_string_cat(app->uart_log_string, app->log_string);
        } else {
            furi_string_cat_printf(app->uart_log_string, "[Waiting for UART data...]\n");
        }

        text_box_set_text(app->text_box, furi_string_get_cstr(app->uart_log_string));
        text_box_set_focus(app->text_box, TextBoxFocusEnd);
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewTextBox);

        // Note: Live updates are now triggered directly by UART worker when data arrives
    }
}

bool evil_bw16_scene_on_event_uart_terminal(void* context, SceneManagerEvent event) {
    EvilBw16App* app = context;

    // Only handle in terminal display mode
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, EvilBw16SceneUartTerminal);
    if(state != 1) return false;

    // Handle refresh timer event
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == EvilBw16EventUartTerminalRefresh) {
        // Only update if log actually changed to reduce CPU usage
        static size_t last_log_size = 0;
        size_t current_log_size = furi_string_size(app->log_string);

        if(current_log_size != last_log_size) {
            // Create display string with header + current log content
            furi_string_reset(app->uart_log_string);
            furi_string_printf(app->uart_log_string, "=== UART TERMINAL ===\n");
            const char* gpio_pins = (app->config.gpio_pins == EvilBw16GpioPins13_14) ? "13↔14" :
                                                                                       "15↔16";
            furi_string_cat_printf(app->uart_log_string, "115200 baud, GPIO %s\n", gpio_pins);
            furi_string_cat_printf(app->uart_log_string, "BW16 PB2←TX  PB1→RX\n");
            furi_string_cat_printf(app->uart_log_string, "Status: ACTIVE\n\n");

            // Add current log content (which is updated by UART worker)
            if(current_log_size > 0) {
                furi_string_cat(app->uart_log_string, app->log_string);
            } else {
                furi_string_cat_printf(app->uart_log_string, "[Waiting for UART data...]\n");
            }

            // Limit total size to prevent memory issues
            if(furi_string_size(app->uart_log_string) > EVIL_BW16_TEXT_BOX_STORE_SIZE - 512) {
                // Keep header and recent content
                const char* gpio_pins_str =
                    (app->config.gpio_pins == EvilBw16GpioPins13_14) ? "13↔14" : "15↔16";
                char header[256];
                snprintf(
                    header,
                    sizeof(header),
                    "=== UART TERMINAL ===\n115200 baud, GPIO %s\nBW16 PB2←TX  PB1→RX\nStatus: ACTIVE\n\n[...truncated...]\n",
                    gpio_pins_str);
                size_t keep_size = EVIL_BW16_TEXT_BOX_STORE_SIZE / 2;

                // Get recent content from app->log_string
                if(current_log_size > keep_size) {
                    furi_string_set_str(app->uart_log_string, header);
                    const char* recent_content =
                        furi_string_get_cstr(app->log_string) + (current_log_size - keep_size);
                    furi_string_cat_str(app->uart_log_string, recent_content);
                }
            }

            // Update the display
            text_box_set_text(app->text_box, furi_string_get_cstr(app->uart_log_string));
            text_box_set_focus(app->text_box, TextBoxFocusEnd);

            last_log_size = current_log_size;
        }

        return true;
    }

    return false;
}

void evil_bw16_scene_on_exit_uart_terminal(void* context) {
    EvilBw16App* app = context;
    text_box_reset(app->text_box);
}
