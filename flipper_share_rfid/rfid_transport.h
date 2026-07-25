#pragma once

// 125 kHz LF RFID transport for Flipper Share: a one-way (carousel) packet pipe
// over the RFID coil. The SENDER is the tag (passive load modulation, clocked by
// the reader's external field); the RECEIVER is the reader (drives the 125 kHz
// carrier and demodulates). See the app README for the full design.
//
//   - fsh_transport_send() (declared in share.h) is wired as the engine's
//     cb_send_bytes; on the tag it queues a frame for the modem, on the reader it
//     is a no-op (the receiver never transmits in v1).
//   - Each decoded packet is delivered to fsh_receive_callback() from the RX
//     worker thread (never from the capture interrupt).

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    RfidTransportModeTag, // sender: load-modulating tag, clocked by the reader field
    RfidTransportModeReader, // receiver: drives the carrier, demodulates
} RfidTransportMode;

// Allocate resources and start the RFID stack in the given role.
void rfid_transport_init(RfidTransportMode mode);

// Stop the RFID stack, reset the pins and free resources. The caller MUST have
// already stopped any thread that calls fsh_transport_send().
void rfid_transport_deinit(void);

// Receiver only: stop the carrier + capture after the transfer finalizes, without
// tearing the transport down (mirrors the NFC app's stop_field). No-op on the tag.
void rfid_transport_stop_field(void);

// True once the tag has seen the reader's field at least once (drives the sender's
// "Waiting for field..." UI hint). Always false / irrelevant on the reader.
bool rfid_transport_tag_field_present(void);
