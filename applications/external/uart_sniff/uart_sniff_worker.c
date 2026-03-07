/**
 * uart_sniff_worker.c — UART capture worker for UART Sniff FAP.
 *
 * Pipeline:
 *   USART ISR (FuriHalSerialRxEventData)
 *     → furi_stream_buffer_send (zero timeout, non-blocking)
 *     → worker thread drain loop
 *     → ring buffer  (mutex-protected)
 *     → main thread reads for display
 */

#include "uart_sniff_worker.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <stdlib.h>
#include <string.h>

#define TAG "UartSniff"

struct UartSniffWorker {
    /* Serial resources */
    FuriHalSerialHandle* serial;
    Expansion* expansion;

    /* ISR → thread bridge */
    FuriStreamBuffer* rx_stream;

    /* Worker thread */
    FuriThread* thread;
    volatile bool running;

    /* Ring buffer — circular, head points to the next write slot */
    uint8_t* ring_buf;
    uint32_t ring_head; /* next write index (mod RING_SIZE) */
    uint32_t ring_fill; /* bytes currently valid in the ring  */
    FuriMutex* ring_mutex;

    /* Statistics */
    volatile uint32_t total_rx;
};

/* -----------------------------------------------------------------------
 * ISR callback — called in interrupt context; must not block.
 * ----------------------------------------------------------------------- */
static void
    uart_sniff_isr_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UartSniffWorker* w = (UartSniffWorker*)context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        /* Zero timeout — ISR must never sleep. Drop byte on overflow. */
        furi_stream_buffer_send(w->rx_stream, &byte, 1, 0);
    }
}

/* -----------------------------------------------------------------------
 * Worker thread — drains the stream buffer into the ring.
 * ----------------------------------------------------------------------- */
static int32_t uart_sniff_worker_thread(void* context) {
    UartSniffWorker* w = (UartSniffWorker*)context;

    FURI_LOG_I(TAG, "Worker started");

    while(w->running) {
        uint8_t byte;
        size_t got = furi_stream_buffer_receive(
            w->rx_stream, &byte, 1, furi_ms_to_ticks(50) /* short timeout keeps loop responsive */
        );

        if(got == 0) continue;

        furi_mutex_acquire(w->ring_mutex, FuriWaitForever);

        w->ring_buf[w->ring_head] = byte;
        w->ring_head = (w->ring_head + 1u) & UART_SNIFF_RING_MASK;
        if(w->ring_fill < UART_SNIFF_RING_SIZE) {
            w->ring_fill++;
        }
        /* When ring_fill == RING_SIZE the oldest byte has just been
         * overwritten — ring_head already stepped past it. */

        furi_mutex_release(w->ring_mutex);

        /* Relaxed — only written here; read as a display counter elsewhere */
        w->total_rx++;
    }

    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

UartSniffWorker* uart_sniff_worker_alloc(uint32_t baud, FuriHalSerialId serial_id) {
    UartSniffWorker* w = malloc(sizeof(UartSniffWorker));
    furi_assert(w);
    memset(w, 0, sizeof(UartSniffWorker));

    /* Disable expansion module so it does not contest the USART line. */
    w->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(w->expansion);

    w->serial = furi_hal_serial_control_acquire(serial_id);
    if(!w->serial) {
        FURI_LOG_W(TAG, "Serial port busy");
        expansion_enable(w->expansion);
        furi_record_close(RECORD_EXPANSION);
        free(w);
        return NULL;
    }

    furi_hal_serial_init(w->serial, baud);

    /* Heap-allocate ring buffer — FAP stack is only ~4 KB. */
    w->ring_buf = malloc(UART_SNIFF_RING_SIZE);
    furi_assert(w->ring_buf);
    w->ring_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(w->ring_mutex);

    w->rx_stream = furi_stream_buffer_alloc(UART_SNIFF_STREAM_SIZE, 1);
    furi_assert(w->rx_stream);

    /* Start thread before enabling ISR so no bytes are dropped. */
    w->running = true;
    w->thread = furi_thread_alloc_ex("UartSniffRx", 1024, uart_sniff_worker_thread, w);
    furi_assert(w->thread);
    furi_thread_start(w->thread);

    furi_hal_serial_async_rx_start(w->serial, uart_sniff_isr_cb, w, false);

    FURI_LOG_I(TAG, "Serial open at %lu baud", (unsigned long)baud);
    return w;
}

void uart_sniff_worker_free(UartSniffWorker* w) {
    furi_assert(w);

    /* Signal the thread to exit, then wait for it to drain the stream. */
    w->running = false;
    furi_thread_join(w->thread);
    furi_thread_free(w->thread);

    furi_hal_serial_async_rx_stop(w->serial);
    furi_hal_serial_deinit(w->serial);
    furi_hal_serial_control_release(w->serial);

    furi_stream_buffer_free(w->rx_stream);
    furi_mutex_free(w->ring_mutex);
    free(w->ring_buf);

    expansion_enable(w->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(w);
}

size_t uart_sniff_worker_read(UartSniffWorker* w, uint8_t* out, size_t len) {
    furi_assert(w);
    furi_assert(out);

    furi_mutex_acquire(w->ring_mutex, FuriWaitForever);

    size_t avail = w->ring_fill;
    if(len > avail) len = avail;

    if(len > 0) {
        /* Oldest byte lives at (ring_head - ring_fill) mod RING_SIZE */
        uint32_t tail = (w->ring_head - (uint32_t)w->ring_fill) & UART_SNIFF_RING_MASK;
        for(size_t i = 0; i < len; i++) {
            out[i] = w->ring_buf[(tail + i) & UART_SNIFF_RING_MASK];
        }
    }

    furi_mutex_release(w->ring_mutex);
    return len;
}

uint32_t uart_sniff_worker_total(const UartSniffWorker* w) {
    furi_assert(w);
    return w->total_rx;
}

void uart_sniff_worker_clear(UartSniffWorker* w) {
    furi_assert(w);
    furi_mutex_acquire(w->ring_mutex, FuriWaitForever);
    w->ring_head = 0;
    w->ring_fill = 0;
    w->total_rx = 0;
    furi_mutex_release(w->ring_mutex);
}
