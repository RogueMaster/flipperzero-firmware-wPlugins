#include "tpms_session.h"
#include "tpms_preset.h"
#include "tpms_lf.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <toolbox/level_duration.h>

#define TAG "TpmsSession"

/** Capacity of the interval buffer. FSK noise comes in a dense stream, so
 * the headroom has to be sizeable: at 20 kBaud this is about 0.1 s of
 * air time. */
#define TPMS_STREAM_CAPACITY 2048

/** How many intervals we take out of the buffer in one go. */
#define TPMS_PUMP_BATCH 128

struct TpmsSession {
    const SubGhzDevice* device;
    FuriStreamBuffer* stream;
    TpmsDecoder* decoder;

    uint32_t frequency;
    TpmsModulation modulation;

    TpmsSessionFrameCallback frame_callback;
    void* frame_context;

    TpmsSessionRawCallback raw_callback;
    void* raw_context;

    volatile uint32_t overruns;
    bool running;

    /* Signal level at the moment the last batch of intervals arrived. */
    float last_rssi;

    /* The decoding buffer lives here rather than on the stack: pump() is
     * called from the CLI command thread, where the stack is modest. */
    LevelDuration batch[TPMS_PUMP_BATCH];
};

static void tpms_session_capture_callback(bool level, uint32_t duration, void* context) {
    /* Interrupt context: just push into the buffer, nothing heavy. */
    TpmsSession* session = context;
    const LevelDuration level_duration = level_duration_make(level, duration);
    if(furi_stream_buffer_send(session->stream, &level_duration, sizeof(LevelDuration), 0) !=
       sizeof(LevelDuration)) {
        session->overruns++;
    }
}

static void tpms_session_frame_callback(const TpmsFrame* frame, void* context) {
    TpmsSession* session = context;
    if(session->frame_callback)
        session->frame_callback(frame, session->last_rssi, session->frame_context);
}

/** Load the radio configuration the protocols of this modulation need. */
static void tpms_session_load_preset(TpmsSession* session) {
    if(session->modulation == TpmsModulationOok) {
        /* The stock OOK preset: 270 kHz of bandwidth around the carrier,
         * which suits chip rates from 50 to 170 us alike. */
        subghz_devices_load_preset(session->device, FuriHalSubGhzPresetOok270Async, NULL);
    } else {
        subghz_devices_load_preset(
            session->device, FuriHalSubGhzPresetCustom, (uint8_t*)tpms_fsk_preset);
    }
    subghz_devices_set_frequency(session->device, session->frequency);
}

TpmsSession* tpms_session_alloc(void) {
    TpmsSession* session = malloc(sizeof(TpmsSession));
    memset(session, 0, sizeof(TpmsSession));

    session->stream = furi_stream_buffer_alloc(
        sizeof(LevelDuration) * TPMS_STREAM_CAPACITY, sizeof(LevelDuration));
    session->decoder = tpms_decoder_alloc(tpms_session_frame_callback, session);
    return session;
}

void tpms_session_free(TpmsSession* session) {
    furi_check(session);
    if(session->running) tpms_session_stop(session);
    tpms_decoder_free(session->decoder);
    furi_stream_buffer_free(session->stream);
    free(session);
}

void tpms_session_set_frame_callback(
    TpmsSession* session,
    TpmsSessionFrameCallback callback,
    void* context) {
    furi_check(session);
    session->frame_callback = callback;
    session->frame_context = context;
}

void tpms_session_set_raw_callback(
    TpmsSession* session,
    TpmsSessionRawCallback callback,
    void* context) {
    furi_check(session);
    session->raw_callback = callback;
    session->raw_context = context;
}

bool tpms_session_start(TpmsSession* session, uint32_t frequency, TpmsModulation modulation) {
    furi_check(session);
    furi_check(!session->running);

    subghz_devices_init();
    session->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!session->device) {
        FURI_LOG_E(TAG, "no cc1101_int device");
        subghz_devices_deinit();
        return false;
    }

    if(!subghz_devices_is_frequency_valid(session->device, frequency)) {
        FURI_LOG_E(TAG, "frequency %lu is not valid", frequency);
        subghz_devices_deinit();
        session->device = NULL;
        return false;
    }

    session->frequency = frequency;
    session->modulation = modulation;

    subghz_devices_begin(session->device);
    subghz_devices_reset(session->device);
    subghz_devices_idle(session->device);
    tpms_session_load_preset(session);

    session->overruns = 0;
    session->last_rssi = 0.0f;
    furi_stream_buffer_reset(session->stream);
    tpms_decoder_set_modulation(session->decoder, modulation);
    tpms_decoder_reset(session->decoder);

    subghz_devices_start_async_rx(session->device, tpms_session_capture_callback, session);
    session->running = true;
    return true;
}

void tpms_session_stop(TpmsSession* session) {
    furi_check(session);
    if(!session->running) return;

    subghz_devices_stop_async_rx(session->device);
    subghz_devices_idle(session->device);
    subghz_devices_sleep(session->device);
    subghz_devices_end(session->device);
    subghz_devices_deinit();

    session->device = NULL;
    session->running = false;
}

bool tpms_session_retune(TpmsSession* session, uint32_t frequency, TpmsModulation modulation) {
    furi_check(session);
    if(!session->running) return false;
    if(session->frequency == frequency && session->modulation == modulation) return true;
    if(!subghz_devices_is_frequency_valid(session->device, frequency)) return false;

    subghz_devices_stop_async_rx(session->device);
    subghz_devices_idle(session->device);

    session->frequency = frequency;
    session->modulation = modulation;
    tpms_session_load_preset(session);

    furi_stream_buffer_reset(session->stream);
    tpms_decoder_set_modulation(session->decoder, modulation);
    tpms_decoder_reset(session->decoder);

    subghz_devices_start_async_rx(session->device, tpms_session_capture_callback, session);
    return true;
}

size_t tpms_session_pump(TpmsSession* session, uint32_t timeout_ms) {
    furi_check(session);

    const size_t received =
        furi_stream_buffer_receive(
            session->stream, session->batch, sizeof(session->batch), timeout_ms) /
        sizeof(LevelDuration);

    /* Sample the level as soon as the data arrives: while we decode a
     * batch the transmission ends and the RSSI drops to the noise floor. */
    if(received > 0 && session->running) {
        session->last_rssi = subghz_devices_get_rssi(session->device);
    }

    for(size_t i = 0; i < received; i++) {
        const bool level = level_duration_get_level(session->batch[i]);
        const uint32_t duration = level_duration_get_duration(session->batch[i]);

        tpms_decoder_feed(session->decoder, level, duration);
        if(session->raw_callback) session->raw_callback(level, duration, session->raw_context);
    }
    return received;
}

void tpms_session_wake_pulse(TpmsSession* session, uint32_t duration_ms) {
    furi_check(session);

    tpms_lf_field_start();
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(duration_ms);
    while(furi_get_tick() < deadline) {
        /* Keep receiving: the sensor answers right after activation. */
        tpms_session_pump(session, 20);
    }
    tpms_lf_field_stop();
}

float tpms_session_get_rssi(TpmsSession* session) {
    furi_check(session);
    if(!session->running) return 0.0f;
    return subghz_devices_get_rssi(session->device);
}

uint32_t tpms_session_get_overruns(TpmsSession* session) {
    furi_check(session);
    return session->overruns;
}
