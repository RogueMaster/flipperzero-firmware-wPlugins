#include "../bt_audio.h"
#include <string.h>

static void bt_audio_version_timeout_cb(void* context) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFirmwareError);
}

static void bt_audio_version_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    bt_audio_set_firmware_response(app, data);

    app->esp32_present = true;
    app->firmware_check_done = true;

    if(app->version_timer) furi_timer_stop(app->version_timer);

    view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventFirmwareOK);
}

void bt_audio_scene_version_on_enter(void* context) {
    BtAudio* app = context;

    app->esp32_present = false;
    bt_audio_set_firmware_response(app, "");

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Checking board...");
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Requesting version");
    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

    if(!app->version_timer) {
        app->version_timer = furi_timer_alloc(bt_audio_version_timeout_cb, FuriTimerTypeOnce, app);
    }
    furi_timer_start(app->version_timer, furi_ms_to_ticks(8000));
    FURI_LOG_I(TAG, "Requesting firmware version (8s timeout)");

    bt_audio_uart_set_rx_callback(app->uart, bt_audio_version_rx_callback);
    bt_audio_uart_tx(app->uart, "VERSION\n");
}

bool bt_audio_scene_version_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtAudioEventFirmwareOK) {
            size_t resp_len = strlen(app->firmware_response);
            FURI_LOG_I(TAG, "Version response: %.*s", (int)resp_len, app->firmware_response);
            furi_string_printf(
                app->text_box_store,
                "ESP32 detected\n\nResponse:\n%s\n\nYou can proceed to scan.",
                app->firmware_response[0] ? app->firmware_response : "Unknown");
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewTextBox);
            consumed = true;
        } else if(event.event == BtAudioEventFirmwareError) {
            FURI_LOG_W(TAG, "Version check timed out");
            furi_string_printf(
                app->text_box_store,
                "No response from ESP32.\n\nCheck wiring and power.\nYou can still try scanning.");
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewTextBox);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_version_on_exit(void* context) {
    BtAudio* app = context;
    widget_reset(app->widget);
    if(app->version_timer) furi_timer_stop(app->version_timer);
}
