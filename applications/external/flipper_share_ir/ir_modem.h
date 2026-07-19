#pragma once

// Pure IR modem codec: converts a byte buffer to/from an array of raw IR
// mark/space timings. No hardware dependency here — this file is deterministic
// and unit-testable on a host. The HAL glue (async RX thread, half-duplex TX)
// lives in ir_transport.c.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ir_modem_config.h"

// One captured IR edge: `level` (true = mark / carrier present) held for
// `duration` microseconds. This mirrors the HAL capture callback signature.
typedef struct {
    uint32_t duration; // microseconds
    bool level; // true = mark, false = space
} IrModemEvent;

// Number of raw timings a frame for `len` bytes will occupy. 0 if len is invalid.
size_t ir_modem_encode_bound(size_t len);

// Encode `len` bytes into raw timings suitable for
// infrared_send_raw_ext(timings, count, /*start_from_mark=*/false, ...).
// Layout (start_from_mark=false => [Space, Mark, Space, ...]):
//   [0] lead gap (Space) [1] sync mark [2] sync space
//   then (data mark, data space) x N, then a trailing end mark.
// Returns the number of timings written, or 0 on error (bad len / small buffer).
size_t ir_modem_encode(const uint8_t* data, size_t len, uint32_t* timings, size_t timings_cap);

// Decode one frame's captured events (everything between two silence timeouts)
// into bytes. Finds the sync mark, quantizes each data space to a level and
// reassembles the bit stream. Integrity (CRC) is the caller's responsibility.
// Returns true and sets *out_len (>0) if a frame was extracted; false otherwise.
bool ir_modem_decode(
    const IrModemEvent* events,
    size_t n_events,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);
