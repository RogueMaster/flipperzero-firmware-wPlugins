#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// LPUART on the same two header pins the I2C bus uses: PC1 is pin 15 and PC0 is
// pin 16, which is why any of this is possible on four wires. Nothing has to be
// re-plugged to go from scanning I2C to listening on a serial line -- and a
// sensor strapped into UART mode puts its Tx on the pad silkscreened SCL, which
// is already sitting in pin 16.
//
// Everything here blocks and is meant to be called from a worker thread, never
// from a draw or input callback.

#define UART_LISTEN_MAX_SAMPLE 32

typedef struct {
    uint32_t baud; // the rate this result was gathered at
    size_t bytes; // how many arrived inside the window
    uint32_t frame_errors; // wrong-rate traffic shows up here, not as bytes
    uint8_t sample[UART_LISTEN_MAX_SAMPLE]; // the first bytes, for the report
    size_t sample_len;
} UartListenResult;

// Three outcomes, not two, and the third is the reason this is an enum. A
// caller handed a plain false cannot tell "listened and heard nothing" from
// "never got to listen", and will eventually print the first while meaning the
// second -- which is the app claiming a measurement it did not make, about the
// one screen a user reaches when they are already being told bad news.
// "Nothing established" is the zero value on purpose, the same way I2CPadUnknown
// is: a zeroed variable, or a caller who forgets to look, then defaults to the
// answer that claims the least.
typedef enum {
    UartListenUnavailable, // never listened: LPUART busy, or not on PC0/PC1
    UartListenSilent, // the sweep ran and every rate came back empty
    UartListenHeard, // something was transmitting; the result is filled in
} UartListenOutcome;

// Why the sweep and not one guess: the rate is not knowable in advance, and a
// wrong one turns real traffic into framing errors rather than silence. The
// winner is the rate that produced bytes *without* them.
//
// abort may be NULL. When it is not, the sweep gives up promptly once it turns
// true, keeping whatever it had heard by then. This exists because the caller
// is a worker thread that gets joined on the way out of the app: a wait that
// ignored the flag would show up to the user as the app taking two seconds to
// close.
UartListenOutcome uart_listen_sweep(
    const uint32_t* bauds,
    size_t baud_count,
    uint32_t window_ms,
    const volatile bool* abort,
    UartListenResult* out);

// Loopback self-test: with a jumper from pin 15 to pin 16 this proves the whole
// path -- expansion service stood down, handle acquired, pinout as documented,
// framing, transmit, the interrupt-context receive, and a clean release. It is
// the only way to test the transport without owning a UART sensor, and it uses
// one wire the user already has.
//
// detail receives a short human-readable line either way; pass NULL to skip.
bool uart_selftest_loopback(uint32_t baud, char* detail, size_t detail_size);
