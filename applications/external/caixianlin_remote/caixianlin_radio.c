#include "caixianlin_radio.h"
#include "caixianlin_protocol.h"
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

// Initialize radio hardware
bool caixianlin_radio_init(CaixianlinRemoteApp* app) {
    subghz_devices_init();
    app->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

    if(app->radio_device) {
        subghz_devices_begin(app->radio_device);
        subghz_devices_reset(app->radio_device);
        subghz_devices_load_preset(app->radio_device, FuriHalSubGhzPresetOok270Async, NULL);
        subghz_devices_set_frequency(app->radio_device, 433920000);
        subghz_devices_idle(app->radio_device);
        FURI_LOG_I(TAG, "Radio initialized");
        return true;
    } else {
        FURI_LOG_E(TAG, "Failed to get radio device");
        return false;
    }
}

// Cleanup radio hardware
void caixianlin_radio_deinit(CaixianlinRemoteApp* app) {
    if(app->is_transmitting) {
        caixianlin_radio_stop_tx(app);
    }
    if(app->is_listening) {
        caixianlin_radio_stop_rx(app);
    }

    if(app->radio_device) {
        subghz_devices_sleep(app->radio_device);
        subghz_devices_end(app->radio_device);
    }
    subghz_devices_deinit();
}

// TX callback
LevelDuration caixianlin_radio_tx_callback(void* context) {
    CaixianlinRemoteApp* app = context;
    TxState* enc = &app->tx_state;

    if(enc->buffer_index >= enc->buffer_len) {
        enc->buffer_index = 0;
    }

    return enc->buffer[enc->buffer_index++];
}

// Start transmission
void caixianlin_radio_start_tx(CaixianlinRemoteApp* app) {
    if(app->is_transmitting || !app->radio_device) return;

    FURI_LOG_I(
        TAG, "TX: ID=%d CH=%d M=%d S=%d", app->station_id, app->channel, app->mode, app->strength);

    caixianlin_protocol_encode_message(app);
    subghz_devices_idle(app->radio_device);

    if(!subghz_devices_start_async_tx(app->radio_device, caixianlin_radio_tx_callback, app)) {
        FURI_LOG_E(TAG, "Failed to start TX");
        return;
    }

    app->is_transmitting = true;
    notification_message(app->notifications, &sequence_set_red_255);
}

// Stop transmission
void caixianlin_radio_stop_tx(CaixianlinRemoteApp* app) {
    if(!app->is_transmitting) return;

    subghz_devices_stop_async_tx(app->radio_device);
    subghz_devices_idle(app->radio_device);

    app->is_transmitting = false;
    notification_message(app->notifications, &sequence_reset_red);
}

// RX capture callback
void caixianlin_radio_rx_callback(bool level, uint32_t duration, void* context) {
    CaixianlinRemoteApp* app = context;
    RxCapture* rx = &app->rx_capture;

    if(!app->is_listening) return;

    // Store timing as signed: positive for HIGH, negative for LOW
    int32_t timing = level ? (int32_t)duration : -(int32_t)duration;
    furi_stream_buffer_send(rx->stream_buffer, &timing, sizeof(int32_t), 0);
}

// Start listening
void caixianlin_radio_start_rx(CaixianlinRemoteApp* app) {
    if(app->is_listening || !app->radio_device) return;

    FURI_LOG_I(TAG, "Starting RX");

    furi_stream_buffer_reset(app->rx_capture.stream_buffer);
    app->rx_capture.work_buffer_len = 0;
    app->rx_capture.processed = 0;
    app->rx_capture.capture_valid = false;

    subghz_devices_idle(app->radio_device);
    subghz_devices_start_async_rx(app->radio_device, caixianlin_radio_rx_callback, app);

    app->is_listening = true;
    notification_message(app->notifications, &sequence_set_blue_255);
}

// Stop listening
void caixianlin_radio_stop_rx(CaixianlinRemoteApp* app) {
    if(!app->is_listening) return;

    FURI_LOG_I(TAG, "Stopping RX");

    subghz_devices_stop_async_rx(app->radio_device);
    subghz_devices_idle(app->radio_device);

    furi_stream_buffer_reset(app->rx_capture.stream_buffer);
    app->rx_capture.work_buffer_len = 0;

    app->is_listening = false;
    notification_message(app->notifications, &sequence_reset_blue);
}
