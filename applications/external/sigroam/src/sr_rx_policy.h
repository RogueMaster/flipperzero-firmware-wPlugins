#pragma once
#include <stdbool.h>
#include <stdint.h>

/* The bit values must match FuriHalSerialRxEvent; the correspondence is pinned by the
 * _Static_assert in sr_io.c.
 * Basis: all six values at furi_hal_serial.h:106-113 are 1<<n (read 2026-08-17). */
enum {
    SR_RX_BIT_DATA = 1u << 0,
    SR_RX_BIT_IDLE = 1u << 1,
    SR_RX_BIT_FRAME = 1u << 2,
    SR_RX_BIT_NOISE = 1u << 3,
    SR_RX_BIT_OVERRUN = 1u << 4,
    SR_RX_BIT_PARITY = 1u << 5,
    SR_RX_BIT_ERRORS = SR_RX_BIT_FRAME | SR_RX_BIT_NOISE | SR_RX_BIT_OVERRUN | SR_RX_BIT_PARITY,
};

typedef struct {
    bool drain; /* Bytes must be read out of the peripheral -- even while closing */
    bool enqueue; /* Allowed to push into the stream buffer */
    bool error; /* This callback carries an error bit */
} SrRxDecision;

/* closing suppresses enqueueing only, never draining: carrying the Data bit without draining
 * leaves RXFNE asserted, and
 * while the interrupt enable is still in place the same ISR retriggers immediately and forever. */
static inline SrRxDecision sr_rx_decide(uint32_t bits, bool closing) {
    const bool has_data = (bits & SR_RX_BIT_DATA) != 0u;
    SrRxDecision d;
    d.drain = has_data;
    d.enqueue = has_data && !closing;
    d.error = (bits & SR_RX_BIT_ERRORS) != 0u;
    return d;
}
