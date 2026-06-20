#pragma once

#include <infrared.h>
#include <infrared_worker.h>
#include "../led_remote_types.h"

// Forward declaration
typedef struct LedRemoteApp LedRemoteApp;

// Internal signal storage (InfraredSignal not available in FAP API)
typedef struct {
    bool is_valid;
    bool is_raw;
    InfraredMessage decoded;
    uint32_t* raw_timings;
    size_t    raw_timings_size;
    uint32_t  raw_frequency;
    float     raw_duty_cycle;
} LedRemoteSignal;

void led_remote_ir_send(LedRemoteApp* app, LedButton button);
void led_remote_ir_set_from_worker(
    LedRemoteApp* app,
    LedButton button,
    InfraredWorkerSignal* received_signal);
void led_remote_ir_signal_clear(LedRemoteSignal* signal);
