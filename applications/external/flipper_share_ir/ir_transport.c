#include "ir_transport.h"
#include "ir_modem.h"
#include "ir_modem_config.h"
#include "ir_share.h" // ish_receive_callback

#include <furi.h>
#include <furi_hal_infrared.h>
#include <infrared_transmit.h> // infrared_send_raw_ext

#define TAG "IrTransport"

// The largest protocol packet must fit in one modem frame.
_Static_assert(
    ISH_PACKET_MAX <= IR_MODEM_MAX_FRAME_BYTES,
    "ISH packet too large for the IR modem frame; raise IR_MODEM_MAX_FRAME_BYTES or lower ISH_DATA_LENGTH");

// RX worker thread flags.
#define IR_TP_FLAG_RX_DATA    0x01u // one or more captured edges are in the stream
#define IR_TP_FLAG_RX_TIMEOUT 0x02u // silence timeout -> frame boundary
#define IR_TP_FLAG_EXIT       0x04u
#define IR_TP_FLAG_ALL        (IR_TP_FLAG_RX_DATA | IR_TP_FLAG_RX_TIMEOUT | IR_TP_FLAG_EXIT)

// Stream buffer depth in events (each event is one packed uint32).
#define IR_TP_STREAM_EVENTS 512u

typedef struct {
    FuriThread* thread;
    FuriStreamBuffer* stream; // ISR -> RX thread, packed (level|duration)
    FuriMutex* tx_mutex; // serializes sends / RX pause-resume
    IrModemEvent* events; // frame accumulator, owned by the RX thread
    size_t ev_count;
    uint32_t* tx_timings; // scratch for the encoder
    uint8_t rx_pkt[IR_MODEM_MAX_FRAME_BYTES]; // decoded bytes, RX thread only
} IrTransport;

static IrTransport* ir = NULL;

// Pack/unpack a captured edge into a single uint32 for the stream buffer.
static inline uint32_t ir_ev_pack(bool level, uint32_t duration) {
    return (level ? 0x80000000u : 0u) | (duration & 0x7FFFFFFFu);
}

// --- ISR callbacks (interrupt context) ------------------------------------

static void ir_tp_capture_isr(void* ctx, bool level, uint32_t duration) {
    IrTransport* t = ctx;
    uint32_t item = ir_ev_pack(level, duration);
    furi_stream_buffer_send(t->stream, &item, sizeof(item), 0);
    furi_thread_flags_set(furi_thread_get_id(t->thread), IR_TP_FLAG_RX_DATA);
}

static void ir_tp_timeout_isr(void* ctx) {
    IrTransport* t = ctx;
    furi_thread_flags_set(furi_thread_get_id(t->thread), IR_TP_FLAG_RX_TIMEOUT);
}

// --- RX worker thread -----------------------------------------------------

static void ir_tp_drain_stream(IrTransport* t) {
    uint32_t item;
    while(furi_stream_buffer_receive(t->stream, &item, sizeof(item), 0) == sizeof(item)) {
        if(t->ev_count < IR_MODEM_MAX_EVENTS) {
            t->events[t->ev_count].level = (item & 0x80000000u) != 0;
            t->events[t->ev_count].duration = item & 0x7FFFFFFFu;
            t->ev_count++;
        }
        // On overflow we simply stop appending; the frame will fail to decode and
        // be dropped, then ev_count resets on the next timeout.
    }
}

static int32_t ir_tp_rx_thread(void* context) {
    IrTransport* t = context;

    while(true) {
        uint32_t flags = furi_thread_flags_wait(IR_TP_FLAG_ALL, FuriFlagWaitAny, FuriWaitForever);
        if((flags & IR_TP_FLAG_ALL) == 0) continue; // spurious / error

        if(flags & IR_TP_FLAG_RX_DATA) {
            ir_tp_drain_stream(t);
        }

        if(flags & IR_TP_FLAG_RX_TIMEOUT) {
            // Make sure any events queued just before the timeout are included.
            ir_tp_drain_stream(t);

            if(t->ev_count >= 3) {
                size_t out_len = 0;
                if(ir_modem_decode(
                       t->events, t->ev_count, t->rx_pkt, sizeof(t->rx_pkt), &out_len)) {
                    ish_receive_callback(t->rx_pkt, out_len);
                }
            }
            t->ev_count = 0; // start a fresh frame
        }

        if(flags & IR_TP_FLAG_EXIT) break;
    }

    return 0;
}

// --- RX arm/disarm helpers ------------------------------------------------

static void ir_tp_rx_arm(IrTransport* t) {
    furi_hal_infrared_async_rx_set_capture_isr_callback(ir_tp_capture_isr, t);
    furi_hal_infrared_async_rx_set_timeout_isr_callback(ir_tp_timeout_isr, t);
    furi_hal_infrared_async_rx_start();
    furi_hal_infrared_async_rx_set_timeout(IR_MODEM_RX_TIMEOUT_US);
}

static void ir_tp_rx_disarm(void) {
    furi_hal_infrared_async_rx_set_timeout_isr_callback(NULL, NULL);
    furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL);
    furi_hal_infrared_async_rx_stop();
}

// --- Public API -----------------------------------------------------------

void ir_transport_init(void) {
    if(ir) {
        FURI_LOG_W(TAG, "already initialized");
        return;
    }
    ir = malloc(sizeof(IrTransport));
    ir->events = malloc(sizeof(IrModemEvent) * IR_MODEM_MAX_EVENTS);
    ir->tx_timings = malloc(sizeof(uint32_t) * IR_MODEM_MAX_TIMINGS);
    ir->ev_count = 0;
    ir->stream =
        furi_stream_buffer_alloc(sizeof(uint32_t) * IR_TP_STREAM_EVENTS, sizeof(uint32_t));
    ir->tx_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ir->thread = furi_thread_alloc_ex("IrRxWorker", 2048, ir_tp_rx_thread, ir);
    furi_thread_start(ir->thread);

    // Use the onboard IR LED for transmission (no external module / OTG needed).
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    ir_tp_rx_arm(ir);

    FURI_LOG_I(
        TAG,
        "init done (bits/sym=%u, ~%u us/sym)",
        (unsigned)IR_MODEM_BITS_PER_SYMBOL,
        (unsigned)(IR_MODEM_MARK_US + IR_MODEM_SPACE_BASE_US +
                   ((IR_MODEM_LEVELS - 1u) * IR_MODEM_SPACE_STEP_US) / 2u));
}

void ir_transport_deinit(void) {
    if(!ir) return;

    // Stop RX HAL first so no ISR fires while we tear down.
    ir_tp_rx_disarm();

    // Stop and join the RX worker thread.
    furi_thread_flags_set(furi_thread_get_id(ir->thread), IR_TP_FLAG_EXIT);
    furi_thread_join(ir->thread);
    furi_thread_free(ir->thread);

    furi_stream_buffer_free(ir->stream);
    furi_mutex_free(ir->tx_mutex);
    free(ir->events);
    free(ir->tx_timings);
    free(ir);
    ir = NULL;

    FURI_LOG_I(TAG, "deinit done");
}

void ir_transport_send(const uint8_t* buf, size_t len) {
    if(!ir || !buf || len == 0) return;

    size_t cnt = ir_modem_encode(buf, len, ir->tx_timings, IR_MODEM_MAX_TIMINGS);
    if(cnt == 0) {
        FURI_LOG_E(TAG, "encode failed (len=%u)", (unsigned)len);
        return;
    }

    furi_mutex_acquire(ir->tx_mutex, FuriWaitForever);
    // Half-duplex: stop listening, transmit (blocking), then resume listening.
    ir_tp_rx_disarm();
    infrared_send_raw_ext(
        ir->tx_timings, (uint32_t)cnt, false, IR_MODEM_CARRIER_HZ, IR_MODEM_DUTY_CYCLE);
    ir_tp_rx_arm(ir);
    furi_mutex_release(ir->tx_mutex);
}
