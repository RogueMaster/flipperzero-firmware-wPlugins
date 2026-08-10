#ifndef DCF77_GPIO_H
#define DCF77_GPIO_H

#include "dcf77_decode.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    Dcf77PulseZero = 0,
    Dcf77PulseOne = 1,
    Dcf77PulseMinuteMark = 2,
    Dcf77PulseNoise = 3,
} Dcf77PulseKind;

/**
 * Classify a carrier-reduction pulse width (milliseconds).
 * Typical: ~100ms → 0, ~200ms → 1; gap >1500ms with no pulse → minute mark
 * (callers pass gap_ms for the silent second).
 */
Dcf77PulseKind dcf77_classify_pulse_ms(uint32_t pulse_ms);

/** Classify a long gap without a pulse (end-of-minute marker). */
Dcf77PulseKind dcf77_classify_gap_ms(uint32_t gap_ms);

typedef struct {
    uint8_t bits[DCF77_FRAME_BITS];
    uint8_t count; /* bits accepted this minute (0..59) */
    bool complete; /* true after minute mark with 59 bits */
} Dcf77BitBuffer;

void dcf77_bit_buffer_reset(Dcf77BitBuffer* buf);

/**
 * Feed a classified pulse. Returns true when a full frame is ready in buf
 * (minute mark after exactly 59 bits). Noise pulses are ignored.
 */
bool dcf77_bit_buffer_feed(Dcf77BitBuffer* buf, Dcf77PulseKind kind);

#endif /* DCF77_GPIO_H */
