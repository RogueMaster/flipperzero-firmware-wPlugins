#include "../bt_audio.h"
#include <string.h>

// Delay before sending SCAN command to ensure UART callback is set up
#define PRE_SCAN_DELAY_MS 50

// Helper function to check if a device MAC is already in the list
static bool bt_audio_is_device_duplicate(BtAudio* app, const char* device_info) {
    // Extract MAC address from device_info (format: "MAC,Name")
    const char* comma = strchr(device_info, ',');
    size_t mac_len = comma ? (size_t)(comma - device_info) : strlen(device_info);

    // Compare with existing devices
    for(uint8_t i = 0; i < app->device_count; i++) {
        const char* existing_comma = strchr(app->device_list[i], ',');
        size_t existing_mac_len = existing_comma ? (size_t)(existing_comma - app->device_list[i]) :
                                                   strlen(app->device_list[i]);

        // Compare MAC addresses (case-insensitive)
        if(mac_len == existing_mac_len &&
           strncasecmp(device_info, app->device_list[i], mac_len) == 0) {
            return true; // Duplicate found
        }
    }
    return false;
}

static void bt_audio_scan_timeout_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventScanTimeout);
}

static void bt_audio_uart_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    // Track UART activity
    app->uart_rx_active = true;
    app->last_rx_time = furi_get_tick();

    // Log all received data for debugging
    FURI_LOG_I(TAG, "RX: %s", data);

    // Parse device info from UART response
    // Expected format: "DEVICE:<MAC>,<Name>"
    if(strncmp(data, "DEVICE:", 7) == 0) {
        if(app->device_count < BT_AUDIO_MAX_DEVICES) {
            const char* device_info = data + 7;

            // Check for duplicate before adding
            if(bt_audio_is_device_duplicate(app, device_info)) {
                FURI_LOG_D(TAG, "Skipping duplicate device: %s", device_info);
            } else {
                strncpy(
                    app->device_list[app->device_count],
                    device_info,
                    BT_AUDIO_DEVICE_NAME_LEN - 1);
                app->device_list[app->device_count][BT_AUDIO_DEVICE_NAME_LEN - 1] = '\0';
                app->device_count++;
                size_t name_len = strlen(app->device_list[app->device_count - 1]);
                FURI_LOG_I(
                    TAG,
                    "Device found: %.*s",
                    (int)name_len,
                    app->device_list[app->device_count - 1]);

                // Send event to update the UI with device count
                view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDeviceFound);
            }
        }
    } else if(strncmp(data, "SCAN_COMPLETE", 13) == 0) {
        // Scan complete, switch to device list
        if(app->scan_timer) furi_timer_stop(app->scan_timer);
        FURI_LOG_I(TAG, "Scan complete, %u device(s)", app->device_count);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventStartScan);
    } else if(strncmp(data, "SCAN_START", 10) == 0) {
        FURI_LOG_I(TAG, "ESP32 acknowledged scan start");
    } else if(strncmp(data, "CMD_RECV:", 9) == 0) {
        // ESP32 echoed our command, confirms communication is working
        FURI_LOG_I(TAG, "ESP32 received: %s", data + 9);
    } else if(strncmp(data, "ERROR:", 6) == 0) {
        // Log errors from ESP32
        FURI_LOG_W(TAG, "ESP32 error: %s", data + 6);
    }
}

void bt_audio_scene_scan_on_enter(void* context) {
    BtAudio* app = context;

    // Reset device list and UART tracking
    app->device_count = 0;
    app->selected_device = -1;
    app->uart_rx_active = false;

    // Set up UART callback
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_uart_rx_callback);

    // Show scanning message
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "Scanning for");
    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "BT devices...");
    widget_add_string_element(
        app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Found: 0 devices");
    widget_add_string_element(
        app->widget, 64, 56, AlignCenter, AlignCenter, FontSecondary, "Timeout: 12s");

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

    // Small delay to ensure UART callback is fully set up before sending command
    // This helps improve first-scan reliability
    furi_delay_ms(PRE_SCAN_DELAY_MS);

    // Send SCAN command to ESP32
    bt_audio_uart_tx(app->uart, "SCAN\n");

    if(!app->scan_timer) {
        app->scan_timer = furi_timer_alloc(bt_audio_scan_timeout_cb, FuriTimerTypeOnce, app);
    }
    furi_timer_start(app->scan_timer, furi_ms_to_ticks(12000));
    FURI_LOG_I(TAG, "Started scan with 12s timeout");
}

bool bt_audio_scene_scan_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventDeviceFound) {
            // Update device count display
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "Scanning for");
            widget_add_string_element(
                app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "BT devices...");

            char count_str[32];
            snprintf(count_str, sizeof(count_str), "Found: %u device(s)", app->device_count);
            widget_add_string_element(
                app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, count_str);

            consumed = true;
        } else if(event.event == BtAudioEventStartScan) {
            if(app->scan_timer) furi_timer_stop(app->scan_timer);
            if(app->device_count > 0) {
                scene_manager_next_scene(app->scene_manager, BtAudioSceneDeviceList);
            } else {
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "No devices");
                widget_add_string_element(
                    app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "found");
                widget_add_string_element(
                    app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Put devices in");
                widget_add_string_element(
                    app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "pairing mode");
            }
            consumed = true;
        } else if(event.event == BtAudioEventScanTimeout) {
            if(app->scan_timer) furi_timer_stop(app->scan_timer);
            if(app->device_count > 0) {
                scene_manager_next_scene(app->scene_manager, BtAudioSceneDeviceList);
            } else {
                FURI_LOG_W(TAG, "Scan timed out, uart_active=%d", app->uart_rx_active);
                widget_reset(app->widget);

                if(!app->uart_rx_active) {
                    // No UART response at all - communication issue
                    widget_add_string_element(
                        app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "No ESP32");
                    widget_add_string_element(
                        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Response!");
                    widget_add_string_element(
                        app->widget,
                        64,
                        36,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Check wiring &");
                    widget_add_string_element(
                        app->widget,
                        64,
                        48,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "ESP32 power");
                    widget_add_string_element(
                        app->widget,
                        64,
                        60,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Run Diagnostics");
                } else {
                    // Got UART response but no devices found
                    widget_add_string_element(
                        app->widget,
                        64,
                        8,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "Scan timed out");
                    widget_add_string_element(
                        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "No devices");
                    widget_add_string_element(
                        app->widget,
                        64,
                        36,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Troubleshooting:");
                    widget_add_string_element(
                        app->widget,
                        64,
                        48,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "- Pairing mode?");
                    widget_add_string_element(
                        app->widget,
                        64,
                        60,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "- Move closer");
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_scan_on_exit(void* context) {
    BtAudio* app = context;
    widget_reset(app->widget);
    if(app->scan_timer) furi_timer_stop(app->scan_timer);
}
