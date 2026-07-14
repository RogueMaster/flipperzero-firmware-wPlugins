#include "../bt_audio.h"
#include <string.h>

static void bt_audio_diag_timeout_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDiagComplete);
}

static void bt_audio_diag_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;

    // Track that UART is receiving data
    app->uart_rx_active = true;
    app->last_rx_time = furi_get_tick();

    // Log all received data for debugging
    FURI_LOG_I(TAG, "DIAG RX: %s", data);

    // Accumulate diagnostic response in buffer
    size_t current_len = strlen(app->diag_buffer);
    size_t remaining = sizeof(app->diag_buffer) - current_len - 1; // -1 for null terminator
    if(remaining > 2) { // Need at least 3 chars: 1 data + newline + null terminator
        // Leave room for newline and null terminator
        size_t max_copy = remaining - 1; // -1 for newline
        size_t copy_len = len < max_copy ? len : max_copy;
        // Copy data using memcpy for efficiency, then manually add newline
        memcpy(app->diag_buffer + current_len, data, copy_len);
        current_len += copy_len;
        app->diag_buffer[current_len] = '\n';
        app->diag_buffer[current_len + 1] = '\0';
    }

    // Check for diagnostic completion
    if(strncmp(data, "DIAG_COMPLETE", 13) == 0 || strncmp(data, "DEBUG_END", 9) == 0) {
        if(app->version_timer) furi_timer_stop(app->version_timer);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventDiagComplete);
    }
}

void bt_audio_scene_diagnostics_on_enter(void* context) {
    BtAudio* app = context;

    // Clear diagnostic buffer
    memset(app->diag_buffer, 0, sizeof(app->diag_buffer));
    app->uart_rx_active = false;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Running");
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Diagnostics...");
    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

    // Set up UART callback
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_diag_rx_callback);

    // Set timeout
    if(!app->version_timer) {
        app->version_timer = furi_timer_alloc(bt_audio_diag_timeout_cb, FuriTimerTypeOnce, app);
    }
    furi_timer_start(app->version_timer, furi_ms_to_ticks(5000));

    // Send DEBUG command to ESP32
    bt_audio_uart_tx(app->uart, "DEBUG\n");
    FURI_LOG_I(TAG, "Requesting diagnostics");
}

bool bt_audio_scene_diagnostics_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventDiagComplete) {
            if(app->version_timer) furi_timer_stop(app->version_timer);

            if(app->uart_rx_active || strlen(app->diag_buffer) > 0) {
                // Got response from ESP32
                // Check real-time 5V GPIO status
                const char* gpio_status = furi_hal_power_is_otg_enabled() ? "5V GPIO: ENABLED" :
                                                                            "5V GPIO: DISABLED";

                furi_string_printf(
                    app->text_box_store,
                    "ESP32 Communication: OK\n\n"
                    "Response:\n%s\n\n"
                    "%s\n\n"
                    "Troubleshooting:\n"
                    "- Ensure BT devices are\n"
                    "  in pairing mode\n"
                    "- Move devices closer\n"
                    "- Try resetting ESP32",
                    strlen(app->diag_buffer) > 0 ? app->diag_buffer : "(empty)",
                    gpio_status);
            } else {
                // No response - UART communication issue
                // Check real-time 5V GPIO status
                const char* gpio_status = furi_hal_power_is_otg_enabled() ? "5V GPIO: ENABLED" :
                                                                            "5V GPIO: DISABLED";

                furi_string_printf(
                    app->text_box_store,
                    "ESP32 Communication: FAIL\n\n"
                    "No UART response!\n\n"
                    "%s\n"
                    "(ESP32 VIN must be wired\n"
                    "to Flipper pin 1)\n\n"
                    "Check wiring:\n"
                    "- 5V(pin 1) -> ESP VIN\n"
                    "- TX(13) -> ESP RX\n"
                    "- RX(14) -> ESP TX\n"
                    "- GND -> ESP GND\n\n"
                    "Check ESP32:\n"
                    "- Is LED lit?\n"
                    "- Was firmware flashed?\n"
                    "- Try resetting ESP32",
                    gpio_status);
            }

            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewTextBox);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_diagnostics_on_exit(void* context) {
    BtAudio* app = context;
    widget_reset(app->widget);
    if(app->version_timer) furi_timer_stop(app->version_timer);
}
