#include "receiver.h"
#include "transmit.h" // for OPENSHOCK_FREQUENCY

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>

#include <stdlib.h>
#include <string.h>

#define RX_BUFFER_SIZE 256

struct OpenshockRx {
    // Pulse buffer (written by ISR callback, read by decode thread)
    OokPulse buffer[RX_BUFFER_SIZE];
    volatile size_t write_count;
    volatile uint16_t pending_high;
    volatile bool has_pending;
    volatile bool packet_ready;

    // Decode thread
    FuriThread* thread;
    volatile bool running;

    // Result
    DecodedShocker result;
    volatile bool has_result;
};

static void rx_callback(bool level, uint32_t duration, void* context) {
    OpenshockRx* rx = context;

    // Don't accumulate while a packet is pending decode
    if(rx->packet_ready) return;

    uint16_t dur = (duration > 65535) ? 65535 : (uint16_t)duration;

    if(level) {
        rx->pending_high = dur;
        rx->has_pending = true;
    } else if(rx->has_pending) {
        rx->has_pending = false;
        size_t idx = rx->write_count;
        if(idx < RX_BUFFER_SIZE) {
            rx->buffer[idx].high_us = rx->pending_high;
            rx->buffer[idx].low_us = dur;
            rx->write_count = idx + 1;
        }

        // Trigger decode on gap (>5ms silence) or buffer nearly full
        if((duration > 5000 && idx > 20) || idx >= RX_BUFFER_SIZE - 1) {
            rx->packet_ready = true;
        }
    }
}

static int32_t rx_thread(void* context) {
    OpenshockRx* rx = context;

    while(rx->running) {
        if(rx->packet_ready) {
            size_t count = rx->write_count;
            DecodedShocker decoded;
            if(openshock_decode(rx->buffer, count, &decoded)) {
                rx->result = decoded;
                rx->has_result = true;
            }
            rx->write_count = 0;
            rx->packet_ready = false;
        }
        furi_delay_ms(10);
    }

    return 0;
}

OpenshockRx* openshock_rx_alloc(void) {
    OpenshockRx* rx = malloc(sizeof(OpenshockRx));
    if(!rx) return NULL;
    memset(rx, 0, sizeof(OpenshockRx));
    return rx;
}

void openshock_rx_free(OpenshockRx* rx) {
    if(!rx) return;
    openshock_rx_stop(rx);
    free(rx);
}

void openshock_rx_start(OpenshockRx* rx) {
    if(!rx || rx->running) return;

    rx->write_count = 0;
    rx->has_pending = false;
    rx->packet_ready = false;
    rx->has_result = false;
    rx->running = true;

    rx->thread = furi_thread_alloc_ex("OpenshockRx", 2048, rx_thread, rx);
    furi_thread_start(rx->thread);

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_hal_subghz_set_frequency_and_path(OPENSHOCK_FREQUENCY);
    furi_hal_subghz_rx();

    furi_hal_subghz_start_async_rx(rx_callback, rx);
}

void openshock_rx_stop(OpenshockRx* rx) {
    if(!rx || !rx->running) return;

    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_sleep();

    rx->running = false;
    furi_thread_join(rx->thread);
    furi_thread_free(rx->thread);
    rx->thread = NULL;
}

bool openshock_rx_get_result(OpenshockRx* rx, DecodedShocker* result) {
    if(!rx || !rx->has_result) return false;
    *result = rx->result;
    rx->has_result = false;
    return true;
}
