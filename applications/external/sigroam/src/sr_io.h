#pragma once
#include <furi.h>
#include <stddef.h>
#include <stdint.h>
#include "sr_source.h" /* The forward declaration of SrIo comes from here; do not typedef it again in this header */

typedef enum {
    SrIoOk = 0,
    SrIoErrState, /* A NULL argument, or open called while already open */
    SrIoErrLogDevice, /* Log Device = Usart, which conflicts with pins 13/14 (V-043) */
    SrIoErrNo5v, /* Neither 5V source reached pin 1: no USB VBUS and the OTG boost never came up (V-072) */
    SrIoErrPortBusy, /* control_acquire returned NULL */
    SrIoErrBadBaud, /* The HAL does not support that baud rate */
    SrIoErrAlloc, /* Allocation failed */
} SrIoStatus;

typedef struct {
    uint32_t rx_bytes; /* Bytes the ISR actually pushed into the stream buffer */
    uint32_t rx_dropped; /* Bytes dropped because the buffer was full */
    uint32_t rx_event_mask; /* Accumulated OR of event bits, for looking up afterwards which ones occurred */
    uint32_t rx_errors; /* Callbacks carrying an error bit (not itemized, ADR-014 decision 7) */
    uint32_t tx_bytes;
    uint32_t rx_max_fill; /* Historical high-water mark of the RX buffer (B). See the sampling note in sr_io_read -- this is a lower bound, not the true peak. */
    uint32_t opens; /* Successful opens */
    uint32_t closes; /* Completed closes */
} SrIoStats;

SrIo* sr_io_alloc(void); /* Allocates buffers only, touches no hardware; returns NULL on failure */
void sr_io_free(SrIo* io); /* Closes first if still open; NULL safe */
SrIoStatus sr_io_open(SrIo* io, uint32_t baud);
/* closing=true + async_rx_stop. No-op when not open or rx_running == false.
 * Does not deinit and does not clear open. Safe to call while the worker is still blocked in receive. */
void sr_io_rx_stop(SrIo* io);
/* If still open, rx_stop first, then deinit/release. Idempotent.
 * The caller must already have joined any thread blocked in sr_io_read. */
void sr_io_close(SrIo* io);
bool sr_io_is_open(const SrIo* io);
size_t sr_io_read(SrIo* io, uint8_t* buf, size_t cap, uint32_t timeout_ms);
/* Must not be called while holding the app mutex (it blocks internally waiting for TX to finish). */
bool sr_io_write(SrIo* io, const void* data, size_t len);
void sr_io_get_stats(const SrIo* io, SrIoStats* out);
const char* sr_io_status_str(SrIoStatus st); /* For logging only; returns a static literal */
