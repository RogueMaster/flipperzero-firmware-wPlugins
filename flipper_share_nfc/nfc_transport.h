#pragma once

// NFC transport: a packet pipe over an ISO14443-3A poller/listener pair.
// Drop-in replacement for the subghz_share / ir_transport transport layer.
//
// Role mapping: the SENDER emulates a card (listener), the RECEIVER acts as
// the reader (poller). The poller drives a continuous command/response
// exchange loop; each exchange carries at most one flipper-share packet in
// each direction (an empty "poll" frame when a side has nothing to say).
//
//   - nfc_transport_send() is wired as the protocol's cb_send_bytes callback;
//     it enqueues the packet for the next exchange.
//   - Each received packet is delivered to nsh_receive_callback() (declared
//     in nfc_share.h) from the NfcWorker thread, exactly like the old radio
//     RX path.
//
// Field loss / re-activation is handled by the NFC stack: the poller keeps
// re-activating the card, and the protocol's block bitmap makes the transfer
// resume where it stopped.

#include <stdint.h>
#include <stddef.h>

typedef enum {
    NfcTransportModeListener, // sender: emulate a card, answer poller exchanges
    NfcTransportModePoller, // receiver: drive the exchange loop
} NfcTransportMode;

// Allocate resources and start the NFC stack in the given role.
void nfc_transport_init(NfcTransportMode mode);

// Stop the NFC stack and free resources. The caller MUST have already stopped
// any thread that calls nfc_transport_send().
void nfc_transport_deinit(void);

// Queue one packet for transmission in the next exchange. Blocks up to
// NFC_TP_SEND_TIMEOUT_MS when the mailbox is full (backpressure); drops the
// packet if the transport is not running or the timeout expires — the
// protocol's ARQ recovers from any loss.
void nfc_transport_send(const uint8_t* buf, size_t len);

// Stop energizing the RF field once the transfer is finished, without tearing
// the transport down (that stays with nfc_transport_deinit on the scene thread).
// Thread-safe: only signals the scheduler. No-op for the listener role.
void nfc_transport_stop_field(void);
