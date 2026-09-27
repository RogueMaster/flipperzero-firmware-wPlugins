#include "uart_link.h"
#include "gt_parse.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdlib.h>

#define UART_BAUD 115200
#define UART_RX_STREAM_SIZE 512
#define UART_LINE_MAX 96
#define UART_WORKER_STACK 1536

#define WORKER_FLAG_STOP (1u << 0)
#define WORKER_FLAG_DATA (1u << 1)

struct UartLink {
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    volatile bool running;
    /* Cleared before anything else in stop(). The worker checks it immediately
     * before every callback, so a teardown cannot be overtaken by a detection
     * that was already in flight. */
    volatile bool accepting;
    volatile uint32_t last_rx_tick;
    volatile bool greeted;

    UartLinkRxCallback rx_cb;
    UartLinkStatusCallback status_cb;
    void* cb_context;
};

UartLink* uart_link_alloc(void) {
    UartLink* link = malloc(sizeof(UartLink));
    memset(link, 0, sizeof(UartLink));
    return link;
}

void uart_link_free(UartLink* link) {
    furi_assert(link);
    uart_link_stop(link);
    free(link);
}

void uart_link_set_callbacks(
    UartLink* link,
    UartLinkRxCallback rx_cb,
    UartLinkStatusCallback status_cb,
    void* context) {
    furi_assert(link);
    link->rx_cb = rx_cb;
    link->status_cb = status_cb;
    link->cb_context = context;
}

static void uart_link_parse_line(UartLink* link, char* line) {
    /* ANY complete line proves the board is alive, including one we do not
     * understand - a future firmware saying something new still counts. */
    link->last_rx_tick = furi_get_tick();

    if(gt_line_is_detection(line)) {
        GtDetection det;
        /* The parser lives in gt_parse.c with no Flipper dependency, so the
         * one piece of this app that reads attacker-reachable bytes can be
         * exercised on a laptop instead of only ever on a device where a bad
         * read shows up as a reboot. See test/test_parse.c. */
        if(!gt_parse_detection(line + 4, &det)) return;
        if(link->rx_cb && link->accepting) {
            link->rx_cb(link->cb_context, det.mac, det.type, det.rssi, det.name);
        }
        return;
    }

    const char* version = NULL;
    if(gt_line_is_hello(line, &version)) {
        link->greeted = true;
        if(link->status_cb && link->accepting) {
            link->status_cb(link->cb_context, true, version);
        }
        return;
    }

    if(gt_line_is_alive(line)) {
        /* Heartbeat. The last_rx_tick above is the whole point of it; a board
         * that has been heartbeating has obviously greeted us at some stage
         * even if we missed the GTHELLO, which is sent once at the board's
         * boot - quite possibly before this app was opened. */
        link->greeted = true;
    }
}

static int32_t uart_link_worker(void* context) {
    UartLink* link = context;
    char line[UART_LINE_MAX];
    size_t pos = 0;
    uint8_t buf[32];

    while(link->running) {
        size_t n = furi_stream_buffer_receive(link->rx_stream, buf, sizeof(buf), 50);
        for(size_t i = 0; i < n; i++) {
            char c = (char)buf[i];
            if(c == '\n' || c == '\r') {
                if(pos > 0) {
                    line[pos] = '\0';
                    uart_link_parse_line(link, line);
                    pos = 0;
                }
            } else if(pos < sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                pos = 0; // overflow - drop the malformed line
            }
        }
    }
    return 0;
}

static void uart_link_rx_isr(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UartLink* link = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(link->rx_stream, &data, 1, 0);
    }
}

void uart_link_start(UartLink* link) {
    furi_assert(link);
    if(link->running) return;

    // The Expansion service squats on the USART by default - hand it over to us.
    link->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(link->expansion);

    link->rx_stream = furi_stream_buffer_alloc(UART_RX_STREAM_SIZE, 1);
    link->last_rx_tick = 0;
    link->greeted = false;
    link->running = true;
    link->accepting = true;

    link->thread = furi_thread_alloc_ex("GhostTagUart", UART_WORKER_STACK, uart_link_worker, link);
    furi_thread_start(link->thread);

    link->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_check(link->serial);
    furi_hal_serial_init(link->serial, UART_BAUD);
    furi_hal_serial_async_rx_start(link->serial, uart_link_rx_isr, link, false);
}

void uart_link_stop(UartLink* link) {
    furi_assert(link);
    if(!link->running) return;

    /* Order matters here, and getting it wrong deadlocks the whole app.
     *
     * Stop accepting callbacks FIRST. The app's detection callback runs on
     * this worker thread and can post to the view dispatcher's queue, which
     * blocks when that queue is full - and the thread that drains it is the
     * GUI thread, which is the thread sitting in furi_thread_join below. A
     * detection arriving during teardown would have hung the Flipper hard
     * enough to need a reboot. */
    link->accepting = false;

    /* Then silence the ISR, so nothing new enters the stream buffer we are
     * about to free. */
    if(link->serial) {
        furi_hal_serial_async_rx_stop(link->serial);
    }

    link->running = false;

    if(link->serial) {
        /* Let the STOP command actually leave the wire. Tearing the peripheral
         * down straight after queueing it truncated the last few bytes, so the
         * board carried on scanning and burning power after the app closed. */
        furi_hal_serial_tx_wait_complete(link->serial);
        furi_hal_serial_deinit(link->serial);
        furi_hal_serial_control_release(link->serial);
        link->serial = NULL;
    }

    if(link->thread) {
        furi_thread_join(link->thread);
        furi_thread_free(link->thread);
        link->thread = NULL;
    }

    if(link->rx_stream) {
        furi_stream_buffer_free(link->rx_stream);
        link->rx_stream = NULL;
    }

    if(link->expansion) {
        expansion_enable(link->expansion);
        furi_record_close(RECORD_EXPANSION);
        link->expansion = NULL;
    }
}

bool uart_link_is_running(UartLink* link) {
    furi_assert(link);
    return link->running;
}

void uart_link_send_command(UartLink* link, const char* cmd) {
    furi_assert(link);
    if(!link->running || !link->serial) return;
    furi_hal_serial_tx(link->serial, (const uint8_t*)cmd, strlen(cmd));
}

uint32_t uart_link_last_rx_tick(UartLink* link) {
    furi_assert(link);
    return link->last_rx_tick;
}

bool uart_link_has_greeted(UartLink* link) {
    furi_assert(link);
    return link->greeted;
}
