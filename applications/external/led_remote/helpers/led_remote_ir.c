#include "led_remote_ir.h"
#include "../led_remote.h"
#include <lib/infrared/worker/infrared_transmit.h>

void led_remote_ir_signal_clear(LedRemoteSignal* signal) {
    if(signal->is_raw && signal->raw_timings) {
        free(signal->raw_timings);
        signal->raw_timings = NULL;
        signal->raw_timings_size = 0;
    }
    signal->is_valid = false;
}

void led_remote_ir_set_from_worker(
    LedRemoteApp* app,
    LedButton button,
    InfraredWorkerSignal* received_signal) {
    LedRemoteSignal* sig = &app->signals[button];
    led_remote_ir_signal_clear(sig);

    if(infrared_worker_signal_is_decoded(received_signal)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received_signal);
        sig->decoded = *msg;
        sig->is_raw = false;
    } else {
        const uint32_t* timings;
        size_t timings_size;
        infrared_worker_get_raw_signal(received_signal, &timings, &timings_size);

        sig->raw_timings = malloc(timings_size * sizeof(uint32_t));
        memcpy(sig->raw_timings, timings, timings_size * sizeof(uint32_t));
        sig->raw_timings_size = timings_size;
        sig->raw_frequency    = INFRARED_COMMON_CARRIER_FREQUENCY;
        sig->raw_duty_cycle   = INFRARED_COMMON_DUTY_CYCLE;
        sig->is_raw = true;
    }

    sig->is_valid = true;
}

void led_remote_ir_send(LedRemoteApp* app, LedButton button) {
    if(!app->signals[button].is_valid) return;
    LedRemoteSignal* sig = &app->signals[button];

    if(!sig->is_raw) {
        infrared_send(&sig->decoded, 1);
    } else {
        infrared_send_raw_ext(
            sig->raw_timings,
            (uint32_t)sig->raw_timings_size,
            true,
            sig->raw_frequency,
            (float)sig->raw_duty_cycle);
    }
}
