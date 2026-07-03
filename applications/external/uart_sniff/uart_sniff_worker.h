/**
 * uart_sniff_worker.h — UART worker interface for UART Sniff FAP.
 *
 * The worker owns the serial handle and associated ISR → thread pipeline.
 * The ring buffer it maintains is read by the main thread under mutex
 * to produce the hex dump display.
 */
#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ring buffer capacity — must be a power of two for cheap masking. */
#define UART_SNIFF_RING_SIZE 4096u
#define UART_SNIFF_RING_MASK (UART_SNIFF_RING_SIZE - 1u)

/* ISR → worker stream buffer depth. */
#define UART_SNIFF_STREAM_SIZE 512u

typedef struct UartSniffWorker UartSniffWorker;

/**
 * Allocate and start the worker.
 *
 * @param baud      Baud rate to configure the serial port at.
 * @param serial_id Which USART channel to use.
 * @return          Allocated worker, or NULL if the serial port is busy.
 */
UartSniffWorker* uart_sniff_worker_alloc(uint32_t baud, FuriHalSerialId serial_id);

/**
 * Stop and free the worker.  Releases the serial port and re-enables
 * the expansion module.
 */
void uart_sniff_worker_free(UartSniffWorker* worker);

/**
 * Copy the most recent `len` bytes from the ring buffer into `out`.
 * Takes the ring mutex internally; safe to call from any thread.
 *
 * @param worker  Worker instance.
 * @param out     Destination buffer (caller-owned).
 * @param len     Maximum bytes to copy.
 * @return        Number of bytes actually copied.
 */
size_t uart_sniff_worker_read(UartSniffWorker* worker, uint8_t* out, size_t len);

/**
 * Total bytes received since the worker was started (or last cleared).
 * Atomic read — no mutex required for a display counter.
 */
uint32_t uart_sniff_worker_total(const UartSniffWorker* worker);

/**
 * Clear the ring buffer (reset head and count).
 * Safe to call from the main thread; takes mutex internally.
 */
void uart_sniff_worker_clear(UartSniffWorker* worker);

#ifdef __cplusplus
}
#endif
