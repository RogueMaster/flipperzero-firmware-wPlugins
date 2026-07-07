#pragma once

// IR transport: a half-duplex, framed byte pipe over the onboard IR LED + TSOP.
// Drop-in replacement for the old subghz_share transport layer.
//
//   - ir_transport_send() is wired as the protocol's cb_send_bytes callback.
//   - Each received, modem-decoded frame is delivered to ish_receive_callback()
//     (declared in ir_share.h), exactly like the old radio RX path.
//
// Only the app worker thread calls ir_transport_send(); reception happens on a
// dedicated internal thread. Because the hardware is half-duplex, a send pauses
// reception, transmits, then resumes reception.

#include <stdint.h>
#include <stddef.h>

// Allocate resources, start the RX worker thread and begin listening.
void ir_transport_init(void);

// Stop listening, join the RX worker thread and free resources. The caller MUST
// have already stopped any thread that calls ir_transport_send().
void ir_transport_deinit(void);

// Transmit one packet. Blocking and half-duplex: pauses RX, transmits, resumes
// RX. `len` may be any size up to IR_MODEM_MAX_FRAME_BYTES.
void ir_transport_send(const uint8_t* buf, size_t len);
