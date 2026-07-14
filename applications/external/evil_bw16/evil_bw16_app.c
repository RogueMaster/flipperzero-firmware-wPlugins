#include "evil_bw16.h"
#include <string.h>
#include <stdlib.h>

// Scene handlers table
void (*const evil_bw16_scene_on_enter_handlers[])(void*) = {
    evil_bw16_scene_on_enter_start,
    evil_bw16_scene_on_enter_main_menu,
    evil_bw16_scene_on_enter_scanner,
    evil_bw16_scene_on_enter_scanner_results,
    evil_bw16_scene_on_enter_attacks,
    evil_bw16_scene_on_enter_attack_config,
    evil_bw16_scene_on_enter_sniffer,
    evil_bw16_scene_on_enter_sniffer_results,
    evil_bw16_scene_on_enter_config,
    evil_bw16_scene_on_enter_device_info,
    evil_bw16_scene_on_enter_uart_terminal,
};

bool (*const evil_bw16_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    evil_bw16_scene_on_event_start,
    evil_bw16_scene_on_event_main_menu,
    evil_bw16_scene_on_event_scanner,
    evil_bw16_scene_on_event_scanner_results,
    evil_bw16_scene_on_event_attacks,
    evil_bw16_scene_on_event_attack_config,
    evil_bw16_scene_on_event_sniffer,
    evil_bw16_scene_on_event_sniffer_results,
    evil_bw16_scene_on_event_config,
    evil_bw16_scene_on_event_device_info,
    evil_bw16_scene_on_event_uart_terminal,
};

void (*const evil_bw16_scene_on_exit_handlers[])(void*) = {
    evil_bw16_scene_on_exit_start,
    evil_bw16_scene_on_exit_main_menu,
    evil_bw16_scene_on_exit_scanner,
    evil_bw16_scene_on_exit_scanner_results,
    evil_bw16_scene_on_exit_attacks,
    evil_bw16_scene_on_exit_attack_config,
    evil_bw16_scene_on_exit_sniffer,
    evil_bw16_scene_on_exit_sniffer_results,
    evil_bw16_scene_on_exit_config,
    evil_bw16_scene_on_exit_device_info,
    evil_bw16_scene_on_exit_uart_terminal,
};

// Scene manager configuration
const SceneManagerHandlers evil_bw16_scene_handlers = {
    .on_enter_handlers = evil_bw16_scene_on_enter_handlers,
    .on_event_handlers = evil_bw16_scene_on_event_handlers,
    .on_exit_handlers = evil_bw16_scene_on_exit_handlers,
    .scene_num = EvilBw16SceneNum,
};

static bool evil_bw16_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    EvilBw16App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool evil_bw16_back_event_callback(void* context) {
    furi_assert(context);
    EvilBw16App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

EvilBw16App* evil_bw16_app_alloc(void) {
    EvilBw16App* app = malloc(sizeof(EvilBw16App));

    // Initialize app structure
    memset(app, 0, sizeof(EvilBw16App));

    // Initialize default configuration
    app->config.cycle_delay = 2000;
    app->config.scan_time = 5000;
    app->config.num_frames = 3;
    app->config.start_channel = 1;
    app->config.scan_cycles = false;
    app->config.led_enabled = true;
    app->config.debug_mode = false;
    app->config.gpio_pins = EvilBw16GpioPins13_14; // Default to pins 13/14

    // Initialize GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, evil_bw16_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, evil_bw16_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize scene manager
    app->scene_manager = scene_manager_alloc(&evil_bw16_scene_handlers, app);

    // Initialize views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBw16ViewMainMenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBw16ViewTextBox, text_box_get_view(app->text_box));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBw16ViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EvilBw16ViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBw16ViewWidget, widget_get_view(app->widget));

    // Allocate text storage
    app->text_box_store = malloc(EVIL_BW16_TEXT_BOX_STORE_SIZE);
    app->text_input_store = malloc(EVIL_BW16_TEXT_INPUT_STORE_SIZE);
    app->text_box_string = furi_string_alloc();
    app->log_string = furi_string_alloc();
    app->uart_log_string = furi_string_alloc();

    // Initialize UART worker
    app->uart_worker = evil_bw16_uart_init(app);
    app->uart_rx_stream = furi_stream_buffer_alloc(EVIL_BW16_UART_RX_BUF_SIZE, 1);

    return app;
}

void evil_bw16_app_free(EvilBw16App* app) {
    furi_assert(app);

    // Stop UART worker
    if(app->uart_worker) {
        evil_bw16_uart_free(app->uart_worker);
    }

    if(app->uart_rx_stream) {
        furi_stream_buffer_free(app->uart_rx_stream);
    }

    // Free text storage
    free(app->text_box_store);
    free(app->text_input_store);
    furi_string_free(app->text_box_string);
    furi_string_free(app->log_string);
    furi_string_free(app->uart_log_string);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, EvilBw16ViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBw16ViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBw16ViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBw16ViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBw16ViewWidget);

    submenu_free(app->submenu);
    text_box_free(app->text_box);
    text_input_free(app->text_input);
    popup_free(app->popup);
    widget_free(app->widget);

    // Free scene manager and view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// Utility functions
void evil_bw16_show_loading(EvilBw16App* app, const char* text) {
    // Use popup instead of loading view
    // Flipper Zero screen is 128x64 pixels
    popup_set_header(app->popup, "Loading", 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 35, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewPopup);
    app->show_loading = true;
}

void evil_bw16_hide_loading(EvilBw16App* app) {
    app->show_loading = false;
}

void evil_bw16_show_popup(EvilBw16App* app, const char* header, const char* text) {
    // Flipper Zero screen is 128x64 pixels
    popup_set_header(app->popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 35, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 3000);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBw16ViewPopup);
}

void evil_bw16_append_log(EvilBw16App* app, const char* text) {
    // Skip empty or very short lines to reduce spam from WebUI
    if(!text || strlen(text) < 3) return;

    furi_string_cat_printf(app->log_string, "%s\n", text);

    // More aggressive log size management for high-volume WebUI data
    size_t current_size = furi_string_size(app->log_string);
    if(current_size > EVIL_BW16_TEXT_BOX_STORE_SIZE - 512) {
        // Remove first half of the log to prevent memory issues
        size_t remove_size = current_size / 2;
        furi_string_right(app->log_string, current_size - remove_size);
        FURI_LOG_D("EvilBw16", "Log trimmed due to size, removed %zu bytes", remove_size);
    }
}

void evil_bw16_clear_log(EvilBw16App* app) {
    furi_string_reset(app->log_string);
}

void evil_bw16_notification_message(EvilBw16App* app, const NotificationSequence* sequence) {
    notification_message(app->notifications, sequence);
}

// Main entry point
int32_t evil_bw16_app(void* p) {
    UNUSED(p);

    EvilBw16App* app = evil_bw16_app_alloc();

    // UART worker starts automatically during initialization

    // Start with the main menu scene
    scene_manager_next_scene(app->scene_manager, EvilBw16SceneStart);

    // Run the app
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    evil_bw16_app_free(app);

    return 0;
}

// Command functions
void evil_bw16_send_command(EvilBw16App* app, const char* command) {
    if(!app || !app->uart_worker || !command) return;

    // Log the command being sent
    FURI_LOG_I("EvilBw16", "Sending command: %s", command);

    // Add to UART log
    furi_string_cat_printf(app->uart_log_string, "TX: %s\n", command);

    // Limit UART log size
    if(furi_string_size(app->uart_log_string) > EVIL_BW16_TEXT_BOX_STORE_SIZE - 256) {
        // Remove first 1/4 of the log
        size_t remove_size = furi_string_size(app->uart_log_string) / 4;
        furi_string_right(
            app->uart_log_string, furi_string_size(app->uart_log_string) - remove_size);
    }

    // Use the new UART send command function
    evil_bw16_uart_send_command(app->uart_worker, command);
}

void evil_bw16_send_scan_command(EvilBw16App* app) {
    evil_bw16_send_command(app, "scan");
}

void evil_bw16_send_attack_start(EvilBw16App* app) {
    evil_bw16_send_command(app, "start deauther");
}

void evil_bw16_send_attack_stop(EvilBw16App* app) {
    evil_bw16_send_command(app, "stop deauther");
}

void evil_bw16_send_sniff_command(EvilBw16App* app, const char* mode) {
    if(!app || !mode) return;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "sniff %s", mode);
    evil_bw16_send_command(app, cmd);
}

void evil_bw16_send_set_target(EvilBw16App* app, const char* targets) {
    if(!app || !targets) return;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "set target %s", targets);
    evil_bw16_send_command(app, cmd);
}

void evil_bw16_send_channel_hop(EvilBw16App* app, bool enable) {
    evil_bw16_send_command(app, enable ? "hop on" : "hop off");
}

void evil_bw16_send_set_channel(EvilBw16App* app, int channel) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "set ch %d", channel);
    evil_bw16_send_command(app, cmd);
}

void evil_bw16_send_info_command(EvilBw16App* app) {
    evil_bw16_send_command(app, "info");
}

void evil_bw16_send_config_to_device(EvilBw16App* app) {
    if(!app) return;

    char cmd[128];

    // Send cycle delay configuration
    snprintf(cmd, sizeof(cmd), "set cycle_delay %lu", app->config.cycle_delay);
    evil_bw16_send_command(app, cmd);

    // Send scan time configuration
    snprintf(cmd, sizeof(cmd), "set scan_time %lu", app->config.scan_time);
    evil_bw16_send_command(app, cmd);

    // Send frames per AP configuration
    snprintf(cmd, sizeof(cmd), "set frames %lu", app->config.num_frames);
    evil_bw16_send_command(app, cmd);

    // Send start channel configuration
    snprintf(cmd, sizeof(cmd), "set start_channel %d", app->config.start_channel);
    evil_bw16_send_command(app, cmd);

    // Send LED configuration
    snprintf(cmd, sizeof(cmd), "set led %s", app->config.led_enabled ? "on" : "off");
    evil_bw16_send_command(app, cmd);

    // Send debug mode configuration
    snprintf(cmd, sizeof(cmd), "set debug %s", app->config.debug_mode ? "on" : "off");
    evil_bw16_send_command(app, cmd);

    FURI_LOG_I("EvilBw16", "Configuration sent to BW16 device");
}

// Response parsing functions
EvilBw16ResponseType evil_bw16_parse_response_type(const char* response) {
    if(!response) return EvilBw16ResponseUnknown;

    if(strncmp(response, "[INFO]", 6) == 0) return EvilBw16ResponseInfo;
    if(strncmp(response, "[ERROR]", 7) == 0) return EvilBw16ResponseError;
    if(strncmp(response, "[DEBUG]", 7) == 0) return EvilBw16ResponseDebug;
    if(strncmp(response, "[CMD]", 5) == 0) return EvilBw16ResponseCommand;
    if(strncmp(response, "[HOP]", 5) == 0) return EvilBw16ResponseChannelHop;
    if(strstr(response, "deauth") || strstr(response, "attack"))
        return EvilBw16ResponseAttackStatus;

    return EvilBw16ResponseUnknown;
}

bool evil_bw16_parse_scan_result(const char* response, EvilBw16Network* network) {
    if(!response || !network) return false;

    // Parse the format: [INFO] Index\tSSID\t\tBSSID\t\tChannel\tRSSI (dBm)\tFrequency
    // Example: [INFO] 0\ttest_network\t\t11:22:33:44:55:66\t\t6\t-45\t2.4GHz

    // Skip "[INFO] " prefix
    const char* data = response;
    if(strncmp(data, "[INFO] ", 7) == 0) {
        data += 7;
    }

    memset(network, 0, sizeof(EvilBw16Network));

    // Manual tab-separated parsing
    const char* start = data;
    const char* end;
    int field = 0;

    while(*start && field < 6) {
        // Find next tab or end of string
        end = strchr(start, '\t');
        if(!end) end = start + strlen(start);

        // Skip empty fields (double tabs)
        if(end == start) {
            start++;
            continue;
        }

        // Extract field value
        size_t len = end - start;
        char field_value[128];
        if(len >= sizeof(field_value)) len = sizeof(field_value) - 1;
        strncpy(field_value, start, len);
        field_value[len] = '\0';

        // Process field based on index
        switch(field) {
        case 0: // Index
            network->index = atoi(field_value);
            break;
        case 1: // SSID
            strncpy(network->ssid, field_value, sizeof(network->ssid) - 1);
            network->ssid[sizeof(network->ssid) - 1] = '\0';
            break;
        case 2: // BSSID
            strncpy(network->bssid, field_value, sizeof(network->bssid) - 1);
            network->bssid[sizeof(network->bssid) - 1] = '\0';
            break;
        case 3: // Channel
            network->channel = atoi(field_value);
            break;
        case 4: // RSSI
            network->rssi = atoi(field_value);
            break;
        case 5: // Frequency
            if(strstr(field_value, "5GHz")) {
                network->band = EvilBw16Band5GHz;
            } else {
                network->band = EvilBw16Band24GHz;
            }
            break;
        }

        field++;
        start = (*end == '\0') ? end : end + 1;
    }

    // Validation - need at least SSID and BSSID
    if(strlen(network->ssid) > 0 && strlen(network->bssid) > 0) {
        return true;
    }

    return false;
}
