#include <furi_hal.h>
#include <furi_hal_infrared.h>

#include <mp_flipper_modflipperzero.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

inline static void on_rx(void* ctx, bool level, uint32_t duration) {
    mp_flipper_infrared_rx_t* session = ctx;

    if(session->pointer == 0 && !level) {
        return;
    }

    if(session->pointer < session->size) {
        session->buffer[session->pointer] = duration;

        session->pointer++;
    } else {
        session->running = false;
    }
}

inline static void on_rx_timeout(void* ctx) {
    mp_flipper_infrared_rx_t* session = ctx;

    session->running = false;
}

inline static FuriHalInfraredTxGetDataState on_tx(void* ctx, uint32_t* duration, bool* level) {
    mp_flipper_infrared_tx_t* session = ctx;

    *duration = session->provider(session->signal, session->index);
    *level = session->level;

    session->index++;

    if(session->index >= session->size) {
        session->index = 0;
        session->repeat--;
    }

    session->level = !session->level;

    return session->repeat > 0 ? FuriHalInfraredTxGetDataStateOk :
                                 FuriHalInfraredTxGetDataStateLastDone;
}

inline uint32_t* mp_flipper_infrared_receive(uint32_t timeout, size_t* length) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    mp_flipper_infrared_rx_t* session = ctx->infrared_rx;

    if(!furi_hal_infrared_is_busy()) {
        session->pointer = 0;
        session->running = true;

        furi_hal_infrared_async_rx_set_capture_isr_callback(on_rx, session);
        furi_hal_infrared_async_rx_set_timeout_isr_callback(on_rx_timeout, session);

        furi_hal_infrared_async_rx_start();

        furi_hal_infrared_async_rx_set_timeout(timeout);

        while(session->running) {
            furi_delay_tick(10);
        }

        furi_hal_infrared_async_rx_stop();

        *length = session->pointer;
    } else {
        *length = 0;
    }

    return session->buffer;
}

inline bool mp_flipper_infrared_transmit(
    void* signal,
    size_t length,
    mp_flipper_infrared_signal_tx_provider callback,
    uint32_t repeat,
    uint32_t frequency,
    float duty,
    bool use_external_pin) {
    if(furi_hal_infrared_is_busy() || length == 0) {
        return false;
    }

    const mp_flipper_context_t* ctx = mp_flipper_context;

    mp_flipper_infrared_tx_t* session = ctx->infrared_tx;

    session->index = 0;
    session->level = true;
    session->provider = callback;
    session->signal = signal;
    session->repeat = repeat;
    session->size = length;

    const FuriHalInfraredTxPin output = use_external_pin ? FuriHalInfraredTxPinExtPA7 :
                                                           FuriHalInfraredTxPinInternal;

    furi_hal_infrared_set_tx_output(output);

    furi_hal_infrared_async_tx_set_data_isr_callback(on_tx, session);

    furi_hal_infrared_async_tx_start(frequency, duty);

    furi_hal_infrared_async_tx_wait_termination();

    return true;
}

inline bool mp_flipper_infrared_is_busy() {
    return furi_hal_infrared_is_busy();
}
