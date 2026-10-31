#pragma once

#include <input/input.h>

/* The GUI consumes input at arrival while the app thread performs synchronous
 * work. Keep the progress view until its old dispatcher input has drained. */
typedef struct {
    volatile bool active;
    volatile bool suspended;
    volatile bool canceled;
    volatile bool exit_requested;
    volatile uint8_t held;
    uint32_t finished_tick;
} FlipPassOpenInputGate;

static inline void
    flippass_open_input_gate_event(FlipPassOpenInputGate* gate, const InputEvent* event) {
    if(!gate->active) return;
    if(gate->suspended) {
        if(event->key == InputKeyBack && event->type == InputTypeLong) {
            gate->canceled = true;
            gate->exit_requested = true;
        }
        return;
    }
    const uint8_t bit = 1U << event->key;
    if(event->type == InputTypePress) gate->held |= bit;
    if(event->type == InputTypeRelease) gate->held &= (uint8_t)~bit;
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort || event->type == InputTypeLong) gate->canceled = true;
        if(event->type == InputTypeLong) gate->exit_requested = true;
    }
}

static inline bool flippass_open_input_gate_drained(
    const FlipPassOpenInputGate* gate,
    uint32_t now,
    uint32_t drain_ticks) {
    return gate->active && (gate->held == 0U || gate->exit_requested) &&
           (uint32_t)(now - gate->finished_tick) >= drain_ticks;
}
