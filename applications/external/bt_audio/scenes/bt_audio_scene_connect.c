#include "../bt_audio.h"
#include <string.h>

// Track if connection failure has already been handled to prevent infinite DISCONNECT loop
// When ESP32 responds with "ERROR: Not connected" after we send DISCONNECT, we don't want
// to send another DISCONNECT
static bool connect_failure_handled = false;

// Maximum connection retry attempts for improved reliability
#define MAX_CONNECTION_RETRIES 3

// Power stabilization delays (milliseconds)
// Time to wait after enabling OTG for voltage to stabilize
#define POWER_STABILIZE_MS  100
// Time to wait for ESP32 to boot after power was restored
// ESP32 boot time is typically 1-2 seconds
#define ESP32_BOOT_DELAY_MS 1500

static void bt_audio_connect_timeout_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventConnectTimeout);
}

static void bt_audio_init_timer_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventConnectReady);
}

static void bt_audio_connect_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    FURI_LOG_I(TAG, "Connect RX: %s", data);

    if(strncmp(data, "CONNECTED", 9) == 0) {
        if(app->connect_timer) furi_timer_stop(app->connect_timer);
        connect_failure_handled = false; // Reset flag on successful connection

        // Show initializing message while audio pipeline stabilizes
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Connected!");
        widget_add_string_element(
            app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Initializing audio...");

        // Use non-blocking timer to give ESP32 time to stabilize audio pipeline
        // This helps prevent the first audio playback delay issue
        if(!app->init_timer) {
            app->init_timer = furi_timer_alloc(bt_audio_init_timer_cb, FuriTimerTypeOnce, app);
        }
        furi_timer_start(app->init_timer, furi_ms_to_ticks(500));
    } else if(strncmp(data, "FAILED", 6) == 0 || strncmp(data, "ERROR:", 6) == 0) {
        // Only handle failure once to prevent infinite loop
        // When we send DISCONNECT to clean up, ESP32 may respond with "ERROR: Not connected"
        // which would trigger another BtAudioEventConnectFailed without this check
        if(connect_failure_handled) {
            FURI_LOG_I(TAG, "Connect failure already handled, ignoring: %s", data);
            return;
        }
        connect_failure_handled = true;
        if(app->connect_timer) furi_timer_stop(app->connect_timer);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventConnectFailed);
    } else if(strncmp(data, "CONNECTING", 10) == 0) {
        // ESP32 acknowledged the connection attempt
        FURI_LOG_I(TAG, "ESP32 attempting connection...");
    } else if(strncmp(data, "STATUS: Authentication successful", 33) == 0) {
        // Pairing was successful - update UI
        FURI_LOG_I(TAG, "Authentication successful");
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "Paired!");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Establishing");
        widget_add_string_element(
            app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "audio link...");
    } else if(strncmp(data, "STATUS: Authentication failed", 29) == 0) {
        // Pairing failed - may need device in pairing mode
        FURI_LOG_W(TAG, "Authentication failed - device may need pairing mode");
    } else if(strncmp(data, "STATUS: Device set as trusted", 29) == 0) {
        // Device marked as trusted connection target
        FURI_LOG_I(TAG, "Device set as trusted target");
    }
}

void bt_audio_scene_connect_on_enter(void* context) {
    BtAudio* app = context;

    // Reset failure handled flag for new connection attempt
    connect_failure_handled = false;

    // Set up UART callback
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_connect_rx_callback);

    // =========================================================================
    // 5V Power Check for All Connection Attempts
    // When USB-C is used to power ESP32 and then disconnected, 5V stays off.
    // This check ensures 5V power is restored before any connection attempt
    // if the user has enabled 5V GPIO in settings.
    // =========================================================================
    if(app->config.enable_5v_gpio) {
        bool power_was_off = !furi_hal_power_is_otg_enabled();

        if(power_was_off) {
            FURI_LOG_I(TAG, "5V power was off, attempting to re-enable for connection...");
            furi_hal_power_enable_otg();
            furi_delay_ms(POWER_STABILIZE_MS);

            if(!furi_hal_power_is_otg_enabled()) {
                // 5V power failed to enable - this usually happens when USB-C is connected
                // In this case, USB-C prevents GPIO 5V output (OTG mode conflict)
                // The ESP32 needs external power when USB is plugged in
                FURI_LOG_W(TAG, "Failed to enable 5V GPIO (USB-C may be blocking OTG mode)");
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "5V GPIO Blocked");
                widget_add_string_element(
                    app->widget,
                    64,
                    22,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "USB-C blocks GPIO power");
                widget_add_string_element(
                    app->widget,
                    64,
                    36,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Unplug USB to use GPIO");
                widget_add_string_element(
                    app->widget,
                    64,
                    50,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "or power ESP32 externally");
                widget_add_string_element(
                    app->widget,
                    64,
                    62,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Press Back to exit");
                view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                return;
            }

            FURI_LOG_I(TAG, "5V power restored for connection");
            // Wait for ESP32 to boot after power restore
            furi_delay_ms(ESP32_BOOT_DELAY_MS);
        }
    }

    // Show connecting message with retry count if this is a retry
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "Connecting to");

    if(app->selected_device >= 0 && app->selected_device < app->device_count) {
        // Extract just the device name from "MAC,Name" format for display
        char display_name[BT_AUDIO_DEVICE_NAME_LEN];
        const char* comma = strchr(app->device_list[app->selected_device], ',');
        if(comma && *(comma + 1) != '\0') {
            strncpy(display_name, comma + 1, sizeof(display_name) - 1);
            display_name[sizeof(display_name) - 1] = '\0';
        } else {
            strncpy(
                display_name, app->device_list[app->selected_device], sizeof(display_name) - 1);
            display_name[sizeof(display_name) - 1] = '\0';
        }

        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, display_name);
    }

    // Show retry attempt if this is a retry
    if(app->connection_retry_count > 0) {
        char retry_text[32];
        snprintf(
            retry_text,
            sizeof(retry_text),
            "Attempt %d/%d",
            app->connection_retry_count + 1,
            MAX_CONNECTION_RETRIES + 1);
        widget_add_string_element(
            app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, retry_text);
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "Please wait...");
    } else {
        widget_add_string_element(
            app->widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "Please wait...");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

    // Start connection timeout timer (15 seconds)
    if(!app->connect_timer) {
        app->connect_timer = furi_timer_alloc(bt_audio_connect_timeout_cb, FuriTimerTypeOnce, app);
    }
    furi_timer_start(app->connect_timer, furi_ms_to_ticks(15000));

    // Send CONNECT command to ESP32
    // Extract MAC address from device info (format: "MAC,Name")
    char mac[BT_AUDIO_MAC_LEN];
    const char* comma = strchr(app->device_list[app->selected_device], ',');
    if(comma) {
        size_t mac_len = comma - app->device_list[app->selected_device];
        // Ensure MAC length is valid and fits in buffer
        if(mac_len > 0 && mac_len < sizeof(mac)) {
            strncpy(mac, app->device_list[app->selected_device], mac_len);
            mac[mac_len] = '\0';

            char cmd[BT_AUDIO_CMD_BUF_SIZE];
            snprintf(cmd, sizeof(cmd), "CONNECT %s\n", mac);
            bt_audio_uart_tx(app->uart, cmd);
            FURI_LOG_I(
                TAG,
                "Sent connect command: %s (attempt %d/%d)",
                cmd,
                app->connection_retry_count + 1,
                MAX_CONNECTION_RETRIES + 1);
        }
    }
}

bool bt_audio_scene_connect_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventConnectReady) {
            // Audio pipeline is ready after initialization delay
            if(app->init_timer) furi_timer_stop(app->init_timer);
            app->is_connected = true;
            app->connection_retry_count = 0; // Reset retry count on successful connection

            // Save the connected MAC address to config and device history
            if(app->selected_device >= 0 && app->selected_device < app->device_count) {
                const char* comma = strchr(app->device_list[app->selected_device], ',');
                if(comma) {
                    size_t mac_len = comma - app->device_list[app->selected_device];
                    if(mac_len > 0 && mac_len < BT_AUDIO_MAC_LEN) {
                        // Extract MAC
                        char mac[BT_AUDIO_MAC_LEN];
                        strncpy(mac, app->device_list[app->selected_device], mac_len);
                        mac[mac_len] = '\0';

                        // Save to last connected MAC
                        strncpy(app->config.last_connected_mac, mac, BT_AUDIO_MAC_LEN - 1);
                        app->config.last_connected_mac[BT_AUDIO_MAC_LEN - 1] = '\0';

                        // Extract device name (after comma) and add to history
                        const char* device_name = comma + 1;
                        bt_audio_device_history_add(app, mac, device_name);
                    }
                }
            }

            // Send saved settings to ESP32 after connection
            // This ensures initial volume and EQ are applied before first playback
            char cmd[64];

            // Send initial volume setting
            snprintf(cmd, sizeof(cmd), "SET_INIT_VOL:%d\n", app->config.initial_volume);
            bt_audio_uart_tx(app->uart, cmd);

            // Send EQ settings if not flat (0 dB)
            if(app->config.eq_bass != 0) {
                snprintf(cmd, sizeof(cmd), "EQ_BASS:%d\n", app->config.eq_bass);
                bt_audio_uart_tx(app->uart, cmd);
            }
            if(app->config.eq_mid != 0) {
                snprintf(cmd, sizeof(cmd), "EQ_MID:%d\n", app->config.eq_mid);
                bt_audio_uart_tx(app->uart, cmd);
            }
            if(app->config.eq_treble != 0) {
                snprintf(cmd, sizeof(cmd), "EQ_TREBLE:%d\n", app->config.eq_treble);
                bt_audio_uart_tx(app->uart, cmd);
            }

            // Send TX power setting
            const char* power_level;
            switch(app->config.tx_power) {
            case BtAudioTxPowerLow:
                power_level = "LOW";
                break;
            case BtAudioTxPowerMedium:
                power_level = "MEDIUM";
                break;
            case BtAudioTxPowerHigh:
                power_level = "HIGH";
                break;
            default:
                power_level = "MAX";
                break;
            }
            snprintf(cmd, sizeof(cmd), "TX_POWER:%s\n", power_level);
            bt_audio_uart_tx(app->uart, cmd);

            scene_manager_next_scene(app->scene_manager, BtAudioSceneControl);
            consumed = true;
        } else if(event.event == BtAudioEventConnect) {
            // Deprecated - kept for backward compatibility, redirects to ConnectReady
            view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventConnectReady);
            consumed = true;
        } else if(event.event == BtAudioEventConnectTimeout) {
            // Connection timed out - check if we should retry
            app->connection_retry_count++;

            if(app->connection_retry_count < MAX_CONNECTION_RETRIES) {
                // Retry connection
                FURI_LOG_I(
                    TAG,
                    "Connection timeout, retrying (%d/%d)...",
                    app->connection_retry_count + 1,
                    MAX_CONNECTION_RETRIES);

                // Send disconnect to clean up ESP32 state
                connect_failure_handled = true;
                bt_audio_uart_tx(app->uart, "DISCONNECT\n");
                furi_delay_ms(500); // Wait for ESP32 to process

                // Re-enter the scene to retry
                connect_failure_handled = false;
                scene_manager_previous_scene(app->scene_manager);
                scene_manager_next_scene(app->scene_manager, BtAudioSceneConnect);
                consumed = true;
            } else {
                // Max retries reached - send disconnect to clean up ESP32 state
                // Mark as handled to prevent ESP32's "ERROR: Not connected" response
                // from triggering another failure event
                connect_failure_handled = true;
                app->is_connected = false;
                app->connection_retry_count = 0; // Reset for next attempt
                bt_audio_uart_tx(app->uart, "DISCONNECT\n");

                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "Connection");
                widget_add_string_element(
                    app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Failed");
                widget_add_string_element(
                    app->widget,
                    64,
                    36,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Device may need");
                widget_add_string_element(
                    app->widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "pairing mode");
                widget_add_string_element(
                    app->widget, 64, 60, AlignCenter, AlignCenter, FontSecondary, "Try again...");
                consumed = true;
            }
        } else if(event.event == BtAudioEventConnectFailed) {
            // Connection failed - check if we should retry
            app->connection_retry_count++;

            if(app->connection_retry_count < MAX_CONNECTION_RETRIES) {
                // Retry connection
                FURI_LOG_I(
                    TAG,
                    "Connection failed, retrying (%d/%d)...",
                    app->connection_retry_count + 1,
                    MAX_CONNECTION_RETRIES);

                // Send disconnect to clean up ESP32 state
                // Note: connect_failure_handled is already set in the RX callback
                app->is_connected = false;
                bt_audio_uart_tx(app->uart, "DISCONNECT\n");
                furi_delay_ms(500); // Wait for ESP32 to process

                // Re-enter the scene to retry
                connect_failure_handled = false;
                scene_manager_previous_scene(app->scene_manager);
                scene_manager_next_scene(app->scene_manager, BtAudioSceneConnect);
                consumed = true;
            } else {
                // Max retries reached - show error
                // Note: connect_failure_handled is already set in the RX callback
                // so subsequent ERROR responses won't trigger this again
                app->is_connected = false;
                app->connection_retry_count = 0; // Reset for next attempt
                bt_audio_uart_tx(app->uart, "DISCONNECT\n");

                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "Connection");
                widget_add_string_element(
                    app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Failed");
                widget_add_string_element(
                    app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "Put device in");
                widget_add_string_element(
                    app->widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "pairing mode");
                widget_add_string_element(
                    app->widget, 64, 60, AlignCenter, AlignCenter, FontSecondary, "and try again");
                consumed = true;
            }
        }
    }

    return consumed;
}

void bt_audio_scene_connect_on_exit(void* context) {
    BtAudio* app = context;
    widget_reset(app->widget);
    if(app->connect_timer) {
        furi_timer_stop(app->connect_timer);
    }
    if(app->init_timer) {
        furi_timer_stop(app->init_timer);
    }
}
