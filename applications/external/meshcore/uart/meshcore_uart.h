/*
 * UART layer: a thin wrapper over furi_hal_serial.
 *
 * Knows nothing about MeshCore — it moves bytes. Both of the Flipper's
 * hardware ports are usable and interchangeable as far as this layer cares:
 *
 *   FuriHalSerialIdUsart   pin 13 = TX, pin 14 = RX
 *   FuriHalSerialIdLpuart  pin 15 = TX, pin 16 = RX
 *
 * GND is pin 18 (or 8 / 11). 115200 8N1 on either.
 *
 * RX arrives in interrupt context and is parked in a stream buffer, so
 * meshcore_uart_rx() blocks the *calling* thread, never the GUI thread.
 * Call it from a worker thread only.
 */
#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MESHCORE_UART_BAUD    115200u
/* Sized for the largest uninterrupted burst a node sends, not for one frame.
 * A GET_CONTACTS reply arrives back to back -- CONTACTS_START, a 151-byte
 * frame per contact, then END_OF_CONTACTS -- so a node with twenty contacts
 * puts over 3 KB on the wire with no gap for the reader to catch up in.
 *
 * Undersizing this does not look like a lost frame, which is what makes it
 * expensive to diagnose: the framer resyncs on the next plausible length field
 * and hands up a record assembled from the tail of one contact and the head of
 * the next. That surfaced as a node named "RO" instead of "ROVER-M". */
#define MESHCORE_UART_RX_BUFSZ 4096u

typedef struct MeshCoreUart MeshCoreUart;

/** Take over one of the ports and start receiving.
 *
 * Disables the expansion-module service for the duration, as required by
 * expansion.h before acquiring a serial handle. The service is global rather
 * than per-port, so this is reference counted — with both ports open it is
 * disabled once and restored only when the last one closes.
 *
 * @param   serial_id  FuriHalSerialIdUsart or FuriHalSerialIdLpuart
 * @return  handle, or NULL if that port is already in use.
 */
MeshCoreUart* meshcore_uart_open(FuriHalSerialId serial_id);

/** Stop receiving, release the USART and restore the expansion service. */
void meshcore_uart_close(MeshCoreUart* uart);

/** Semi-blocking transmit: returns once the bytes are in the TX pipe. */
void meshcore_uart_tx(MeshCoreUart* uart, const uint8_t* data, size_t len);

/** Read whatever has arrived, waiting up to timeout_ms for the first byte.
 *
 * @return  number of bytes copied into buf (0 on timeout).
 */
size_t meshcore_uart_rx(MeshCoreUart* uart, uint8_t* buf, size_t cap, uint32_t timeout_ms);

/** Drop anything buffered — used to resync before a fresh handshake. */
void meshcore_uart_rx_flush(MeshCoreUart* uart);

/** Count of framing/noise/overrun/parity errors seen since open.
 *
 * A non-zero value with no valid frames almost always means a wiring or baud
 * mismatch, which is worth telling the user about. */
uint32_t meshcore_uart_rx_errors(const MeshCoreUart* uart);
