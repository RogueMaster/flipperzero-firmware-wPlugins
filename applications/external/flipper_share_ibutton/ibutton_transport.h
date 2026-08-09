#pragma once

// 1-Wire iButton transport for Flipper Share: a packet pipe over the iButton
// pad (pin 17 / PB14). Point-to-point link, exactly two devices, no ROM search.
//
// Role mapping (mirrors the other transports, where the sender is the passive
// side): the RECEIVER is the 1-Wire HOST and drives the bus and all timing; the
// SENDER is the 1-Wire SLAVE (emulator) and answers in read slots.
//
//   - fsh_transport_send() (declared in share.h) is wired as the engine's
//     cb_send_bytes; it enqueues the packet for the next transaction.
//   - Each received packet is delivered to fsh_receive_callback() (declared in
//     share.h) from a thread context (the RX worker on the slave, the host
//     worker on the host) — never from the 1-Wire interrupt.
//
// Contact bounce / separation is handled by the link layer: every transaction
// is a fresh reset+presence, and the engine's block bitmap resumes the transfer
// after any drop.

#include <stdint.h>
#include <stddef.h>

typedef enum {
    IbtnTransportModeSlave, // sender: 1-Wire slave (emulator), answers the host
    IbtnTransportModeHost, // receiver: 1-Wire host, drives the bus
} IbtnTransportMode;

// Allocate resources and start the 1-Wire stack in the given role.
void ibutton_transport_init(IbtnTransportMode mode);

// Stop the 1-Wire stack and free resources. The caller MUST have already
// stopped any thread that calls fsh_transport_send().
void ibutton_transport_deinit(void);

// Pause bus activity once the transfer is finished, without tearing the
// transport down (that stays with ibutton_transport_deinit on the scene
// thread). Thread-safe: only sets a flag observed by the host worker. No-op for
// the slave role (the emulation simply stops answering after deinit).
void ibutton_transport_stop_field(void);
