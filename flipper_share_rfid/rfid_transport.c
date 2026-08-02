#include "rfid_transport.h"
#include "rfid_modem.h"
#include "share.h" // FSH_PACKET_MAX, fsh_transport_send / fsh_receive_callback

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rfid.h>

#define TAG "RfidTransport"

// Half-buffer size (pairs) and the total DMA buffer length.
#define RFID_TP_HALF ((size_t)RFID_TP_DMA_HALF_PAIRS)
#define RFID_TP_TOTAL (RFID_TP_HALF * 2u)

// DMA-refill flags posted by the emulate ISR (mirrors lfrfid_raw_worker).
#define RFID_TP_FLAG_HALF 0u // half-transfer -> refill the first half
#define RFID_TP_FLAG_FULL 1u // transfer-complete -> refill the second half

typedef struct {
    uint8_t len;
    uint8_t data[FSH_PACKET_MAX];
} RfidTpPacket;

typedef struct {
    RfidTransportMode mode;
    volatile bool worker_stop;

    // Tag (sender)
    FuriThread* tag_worker;
    FuriStreamBuffer* dma_flags; // emulate ISR -> tag worker (half/full flags)
    uint32_t* dma_dur; // TIM2 ARR values (carrier cycles - 1), length RFID_TP_TOTAL
    uint32_t* dma_pulse; // TIM2 CCR3 values (carrier cycles), length RFID_TP_TOTAL
    RfidModemEnc enc; // touched only by the tag worker (refill) -> no ISR race
    FuriMessageQueue* tx_q; // single-slot pending frame (send -> refill)
    volatile bool emulating;

    // Reader (receiver)
    FuriThread* rx_worker;
    FuriStreamBuffer* rx_stream; // capture ISR -> rx worker (packed events)
    // Decoded packets are handed to a separate delivery thread so the engine's
    // storage I/O (file create on ANNOUNCE, block writes on DATA) never stalls the
    // capture-drain loop — a stall there overflows rx_stream and drops real frames.
    FuriMessageQueue* deliver_q;
    FuriThread* deliver_worker;
    RfidModemDec dec; // touched only by the rx worker
    volatile bool field_on;
    // Capture-adapter state (rx worker only): the HAL reports PWM-style pairs
    // (high-pulse width on the falling edge, full period on the rising edge), not
    // constant-level run lengths, so the low run is reconstructed as period - high.
    uint32_t rx_last_high;
    bool rx_have_high;
    // Count of complete frames the modem has decoded — used only by the RX worker's
    // self-heal (reset the decoder if it stops producing frames for a while).
    volatile uint32_t rx_frames;
} RfidTransport;

// Owned by the scene lifecycle: init in on_enter, deinit in on_exit.
static RfidTransport* rfid_tp = NULL;

// ===== Engine -> transport (TX) ==============================================

void fsh_transport_send(const uint8_t* buf, size_t len) {
    RfidTransport* tp = rfid_tp;
    if(!tp) return; // not running — drop, carousel re-sends
    if(tp->mode != RfidTransportModeTag) return; // receiver never transmits (v1 carousel)
    if(len < 1 || len > FSH_PACKET_MAX) {
        FURI_LOG_E(TAG, "bad packet length %zu", len);
        return;
    }

    RfidTpPacket pkt;
    pkt.len = (uint8_t)len;
    memcpy(pkt.data, buf, len);
    // Single-slot mailbox: blocks while the modem is still draining the previous
    // frame (backpressure that paces the engine's carousel loop). Drops on timeout
    // (no field -> the DMA is not clocked -> the encoder never drains); the
    // carousel re-sends the block next pass.
    if(furi_message_queue_put(tp->tx_q, &pkt, furi_ms_to_ticks(RFID_TP_SEND_TIMEOUT_MS)) !=
       FuriStatusOk) {
        FURI_LOG_D(TAG, "tx slot busy, frame dropped");
    }
}

// ===== Tag (sender) side =====================================================

// Emulate DMA interrupt: only post which half needs refilling (all real work is
// done by the tag worker in thread context — same split as lfrfid_raw_worker).
static void rfid_tp_dma_isr(bool half, void* context) {
    RfidTransport* tp = context;
    uint32_t flag = half ? RFID_TP_FLAG_HALF : RFID_TP_FLAG_FULL;
    furi_stream_buffer_send(tp->dma_flags, &flag, sizeof(flag), 0);
}

// Fill one DMA slot from the encoder (thread context). Pulls the next frame from
// the single-slot mailbox when the encoder runs dry, else emits idle carrier.
static void rfid_tp_fill_slot(RfidTransport* tp, size_t idx) {
    uint32_t d = 0, p = 0;
    if(!rfid_modem_enc_next(&tp->enc, &d, &p)) {
        RfidTpPacket pkt;
        if(furi_message_queue_get(tp->tx_q, &pkt, 0) == FuriStatusOk) {
            rfid_modem_enc_set_frame(&tp->enc, pkt.data, pkt.len);
            if(!rfid_modem_enc_next(&tp->enc, &d, &p)) {
                d = RFID_MODEM_RF_K;
                p = 0;
            }
        } else {
            d = RFID_MODEM_RF_K; // idle: load off for one bit period
            p = 0;
        }
    }
    // TIM2 ARR = period cycles - 1, CCR3 = load-on cycles.
    tp->dma_dur[idx] = (d > 0) ? (d - 1u) : 0u;
    tp->dma_pulse[idx] = p;
}

static void rfid_tp_fill_half(RfidTransport* tp, size_t start) {
    for(size_t i = 0; i < RFID_TP_HALF; i++) rfid_tp_fill_slot(tp, start + i);
}

static int32_t rfid_tp_tag_worker(void* context) {
    RfidTransport* tp = context;

    // Phase 1: wait for the reader's field (TIM2 is the field counter here); the
    // tag can only load-modulate while the reader's field clocks its emulate timer.
    while(!tp->worker_stop) {
        uint32_t freq = 0;
        if(furi_hal_rfid_field_is_present(&freq)) break;
        furi_delay_ms(50);
    }
    if(tp->worker_stop) return 0;

    // Hand TIM2 from field-detect to the emulate DMA, prefill, and start.
    furi_hal_rfid_field_detect_stop();
    rfid_tp_fill_half(tp, 0);
    rfid_tp_fill_half(tp, RFID_TP_HALF);
    furi_hal_rfid_tim_emulate_dma_start(
        tp->dma_dur, tp->dma_pulse, RFID_TP_TOTAL, rfid_tp_dma_isr, tp);
    tp->emulating = true;

    // Phase 2: refill the inactive half whenever the DMA signals.
    while(!tp->worker_stop) {
        uint32_t flag = 0;
        size_t n = furi_stream_buffer_receive(tp->dma_flags, &flag, sizeof(flag), 50);
        if(n == sizeof(flag)) {
            size_t start = (flag == RFID_TP_FLAG_FULL) ? RFID_TP_HALF : 0;
            rfid_tp_fill_half(tp, start);
        }
    }
    return 0;
}

// ===== Reader (receiver) side ================================================

// Capture interrupt: pack (level, duration_us) into a word and hand it to the RX
// worker. No decoding in the ISR (same event-packing split as ir_transport).
static void rfid_tp_capture_isr(bool level, uint32_t duration, void* context) {
    RfidTransport* tp = context;
    uint32_t e = (level ? 0x80000000u : 0u) | (duration & 0x7FFFFFFFu);
    furi_stream_buffer_send(tp->rx_stream, &e, sizeof(e), 0);
}

// Feed one reconstructed constant-level run to the decoder and dispatch a
// completed packet.
static void rfid_tp_feed_run(RfidTransport* tp, bool level, uint32_t dur_us, uint8_t* pkt) {
    size_t len = rfid_modem_dec_feed(&tp->dec, level, dur_us, pkt, FSH_PACKET_MAX);
    if(len) {
        tp->rx_frames++; // progress signal for the RX worker's self-heal
        // Hand off to the delivery thread instead of calling fsh_receive_callback
        // here — its storage I/O must not block this capture-drain loop. Drop if
        // the delivery queue is full (the carousel re-sends the frame).
        RfidTpPacket p;
        p.len = (uint8_t)len;
        memcpy(p.data, pkt, len);
        furi_message_queue_put(tp->deliver_q, &p, 0);
    }
}

// Delivers decoded packets to the engine (which does the storage I/O), off the
// capture-drain path.
static int32_t rfid_tp_deliver_worker(void* context) {
    RfidTransport* tp = context;
    RfidTpPacket p;
    while(!tp->worker_stop) {
        if(furi_message_queue_get(tp->deliver_q, &p, furi_ms_to_ticks(50)) == FuriStatusOk) {
            fsh_receive_callback(p.data, p.len);
        }
    }
    return 0;
}

static int32_t rfid_tp_rx_worker(void* context) {
    RfidTransport* tp = context;
    uint8_t pkt[FSH_PACKET_MAX];
    uint32_t batch[64]; // drain many capture events per syscall

    // Self-heal: if the modem stops decoding frames for a while, force the decoder
    // + adapter back to a clean sync-hunt. In dense noise the decoder can false-lock
    // and then stay blind in READ with no gap to reset it.
    uint32_t last_check = furi_get_tick();
    uint32_t last_frames = tp->rx_frames;

    while(!tp->worker_stop) {
        // Batch-read: at ~50k noise edges/s the per-event FreeRTOS call overhead of
        // reading one word at a time let the stream buffer overflow and drop real
        // frame edges. Pulling up to 64 events per call keeps the drain loop ahead
        // of the noise so real frames survive.
        size_t got =
            furi_stream_buffer_receive(tp->rx_stream, batch, sizeof(batch), furi_ms_to_ticks(50));
        size_t nev = got / sizeof(uint32_t);

        for(size_t i = 0; i < nev; i++) {
            uint32_t ev = batch[i];
            // PWM-style capture: (true, w) = HIGH pulse width on the falling edge,
            // (false, p) = full period on the rising edge. The low run is p - w.
            bool level = (ev & 0x80000000u) != 0;
            uint32_t dur = ev & 0x7FFFFFFFu;

            if(level) {
                tp->rx_last_high = dur;
                tp->rx_have_high = true;
                rfid_tp_feed_run(tp, true, dur, pkt);
            } else if(tp->rx_have_high && dur > tp->rx_last_high) {
                rfid_tp_feed_run(tp, false, dur - tp->rx_last_high, pkt);
            } else {
                // Period <= last high, or none seen yet (idle): treat as a gap.
                rfid_tp_feed_run(tp, false, RFID_MODEM_GAP_US + 1u, pkt);
                tp->rx_have_high = false;
            }
        }

        uint32_t now = furi_get_tick();
        if(now - last_check > furi_ms_to_ticks(1500)) {
            if(tp->rx_frames == last_frames) {
                // No frame decoded in the last window -> break any stuck state.
                rfid_modem_dec_reset(&tp->dec);
                tp->rx_have_high = false;
            }
            last_frames = tp->rx_frames;
            last_check = now;
        }
    }
    return 0;
}

// ===== Public API ============================================================

void rfid_transport_init(RfidTransportMode mode) {
    furi_assert(rfid_tp == NULL);

    RfidTransport* tp = malloc(sizeof(RfidTransport));
    memset(tp, 0, sizeof(*tp));
    tp->mode = mode;
    tp->worker_stop = false;

    if(mode == RfidTransportModeTag) {
        tp->tx_q = furi_message_queue_alloc(1, sizeof(RfidTpPacket)); // single slot
        tp->dma_flags = furi_stream_buffer_alloc(sizeof(uint32_t) * 8u, sizeof(uint32_t));
        tp->dma_dur = malloc(sizeof(uint32_t) * RFID_TP_TOTAL);
        tp->dma_pulse = malloc(sizeof(uint32_t) * RFID_TP_TOTAL);

        furi_hal_rfid_field_detect_start(); // "waiting for field" phase
        tp->tag_worker = furi_thread_alloc_ex("RfidTagWorker", 2048, rfid_tp_tag_worker, tp);
        furi_thread_start(tp->tag_worker);
    } else {
        rfid_modem_dec_reset(&tp->dec);
        tp->rx_stream = furi_stream_buffer_alloc(RFID_TP_RX_STREAM_SIZE, sizeof(uint32_t));
        tp->deliver_q = furi_message_queue_alloc(8, sizeof(RfidTpPacket));
        tp->deliver_worker =
            furi_thread_alloc_ex("RfidDeliver", 2048, rfid_tp_deliver_worker, tp);
        furi_thread_start(tp->deliver_worker);
        tp->rx_worker = furi_thread_alloc_ex("RfidRxWorker", 2048, rfid_tp_rx_worker, tp);
        furi_thread_start(tp->rx_worker);

        furi_hal_rfid_tim_read_start(125000.0f, 0.5f);
        // Let the field ring up and the demodulator settle before capturing, so
        // the first edges are clean (the stock LF reader waits here too).
        furi_delay_ms(50);
        furi_hal_rfid_tim_read_capture_start(rfid_tp_capture_isr, tp);
        tp->field_on = true;
    }

    rfid_tp = tp; // publish only when fully started
    FURI_LOG_I(TAG, "started as %s", mode == RfidTransportModeTag ? "tag" : "reader");
}

void rfid_transport_deinit(void) {
    RfidTransport* tp = rfid_tp;
    if(!tp) return;
    rfid_tp = NULL; // sends become no-ops first

    tp->worker_stop = true;

    if(tp->mode == RfidTransportModeTag) {
        if(tp->tag_worker) {
            furi_thread_join(tp->tag_worker);
            furi_thread_free(tp->tag_worker);
        }
        if(tp->emulating)
            furi_hal_rfid_tim_emulate_dma_stop();
        else
            furi_hal_rfid_field_detect_stop();
        if(tp->dma_flags) furi_stream_buffer_free(tp->dma_flags);
        if(tp->tx_q) furi_message_queue_free(tp->tx_q);
        if(tp->dma_dur) free(tp->dma_dur);
        if(tp->dma_pulse) free(tp->dma_pulse);
    } else {
        if(tp->field_on) {
            furi_hal_rfid_tim_read_capture_stop();
            furi_hal_rfid_tim_read_stop();
            tp->field_on = false;
        }
        if(tp->rx_worker) {
            furi_thread_join(tp->rx_worker); // stops decoding/enqueuing first
            furi_thread_free(tp->rx_worker);
        }
        if(tp->deliver_worker) {
            furi_thread_join(tp->deliver_worker); // then drain deliveries to the engine
            furi_thread_free(tp->deliver_worker);
        }
        if(tp->rx_stream) furi_stream_buffer_free(tp->rx_stream);
        if(tp->deliver_q) furi_message_queue_free(tp->deliver_q);
    }

    furi_hal_rfid_pins_reset();
    free(tp);
    FURI_LOG_I(TAG, "stopped");
}

void rfid_transport_stop_field(void) {
    RfidTransport* tp = rfid_tp;
    if(!tp || tp->mode != RfidTransportModeReader) return;
    if(tp->field_on) {
        furi_hal_rfid_tim_read_capture_stop();
        furi_hal_rfid_tim_read_stop();
        tp->field_on = false;
    }
}

