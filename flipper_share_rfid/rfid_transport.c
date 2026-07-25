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
    volatile bool field_present;
    volatile bool emulating;

    // Reader (receiver)
    FuriThread* rx_worker;
    FuriStreamBuffer* rx_stream; // capture ISR -> rx worker (packed events)
    RfidModemDec dec; // touched only by the rx worker
    volatile bool field_on;
    // Capture-adapter state (rx worker only): the HAL reports PWM-style pairs
    // (high-pulse width on the falling edge, full period on the rising edge), not
    // constant-level run lengths, so the low run is reconstructed as period - high.
    uint32_t rx_last_high;
    bool rx_have_high;
    // Diagnostics (reader): raw capture edges seen in the ISR and complete packets
    // the modem decoded. Surfaced on the receive "waiting" screen to localize a
    // no-reception (0 edges = no coupling/capture; edges but 0 packets = decode).
    volatile uint32_t rx_events;
    volatile uint32_t rx_frames;
    volatile uint32_t rx_valid; // decoded frames that pass the packet CRC16 (real, not noise)
    volatile uint32_t rx_announce; // valid frames whose type is ANNOUNCE
} RfidTransport;

// How long the tag worker tolerates a stalled emulate DMA (no half/full
// callback, i.e. the external field stopped clocking TIM2) before declaring the
// field lost. A half buffer plays in ~65-80 ms, so a few of those means gone.
#define RFID_TP_FIELD_LOST_MS 400u

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

    // Phase 1: wait for the reader's field (TIM2 is the field counter here).
    while(!tp->worker_stop) {
        uint32_t freq = 0;
        if(furi_hal_rfid_field_is_present(&freq)) {
            tp->field_present = true;
            break;
        }
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

    // Phase 2: refill the inactive half whenever the DMA signals. The emulate
    // TIM2 is clocked by the reader's external field, so the DMA callback stops
    // firing when the devices are separated — use that stall to detect a lost
    // link (the carousel has no return channel) and drive the "Waiting for
    // field..." UI, mirroring how the other transports notice a dropped peer.
    uint32_t last_refill = furi_get_tick();
    while(!tp->worker_stop) {
        uint32_t flag = 0;
        size_t n = furi_stream_buffer_receive(tp->dma_flags, &flag, sizeof(flag), 50);
        if(n == sizeof(flag)) {
            size_t start = (flag == RFID_TP_FLAG_FULL) ? RFID_TP_HALF : 0;
            rfid_tp_fill_half(tp, start);
            last_refill = furi_get_tick();
            tp->field_present = true;
        } else if(furi_get_tick() - last_refill > furi_ms_to_ticks(RFID_TP_FIELD_LOST_MS)) {
            tp->field_present = false;
        }
    }
    return 0;
}

// ===== Reader (receiver) side ================================================

// Capture interrupt: pack (level, duration_us) into a word and hand it to the RX
// worker. No decoding in the ISR (same event-packing split as ir_transport).
static void rfid_tp_capture_isr(bool level, uint32_t duration, void* context) {
    RfidTransport* tp = context;
    tp->rx_events++; // diagnostic: proves the comparator/capture is firing at all
    uint32_t e = (level ? 0x80000000u : 0u) | (duration & 0x7FFFFFFFu);
    furi_stream_buffer_send(tp->rx_stream, &e, sizeof(e), 0);
}

// Feed one reconstructed constant-level run to the decoder and dispatch a
// completed packet.
// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) — same as the engine's, used only
// to tell real decoded packets from noise false-locks in the diagnostics.
static uint16_t rfid_tp_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void rfid_tp_feed_run(RfidTransport* tp, bool level, uint32_t dur_us, uint8_t* pkt) {
    size_t len = rfid_modem_dec_feed(&tp->dec, level, dur_us, pkt, FSH_PACKET_MAX);
    if(len) {
        tp->rx_frames++; // diagnostic: modem decoded a complete frame
        // Distinguish real packets from noise false-locks: check version + CRC16.
        if(len >= FSH_HEADER_LENGTH + FSH_CRC_LENGTH && pkt[0] == 2) {
            uint16_t calc = rfid_tp_crc16(pkt, len - FSH_CRC_LENGTH);
            uint16_t got = (uint16_t)pkt[len - 2] | ((uint16_t)pkt[len - 1] << 8);
            if(calc == got) {
                tp->rx_valid++;
                if(pkt[FSH_HEADER_LENGTH - 1] == FSH_PKT_ANNOUNCE) tp->rx_announce++;
            }
        }
        fsh_receive_callback(pkt, len);
    }
}

static int32_t rfid_tp_rx_worker(void* context) {
    RfidTransport* tp = context;
    uint8_t pkt[FSH_PACKET_MAX];

    while(!tp->worker_stop) {
        uint32_t e = 0;
        size_t n = furi_stream_buffer_receive(tp->rx_stream, &e, sizeof(e), furi_ms_to_ticks(50));
        if(n != sizeof(e)) continue;

        // The capture HAL reports PWM-style timing, NOT constant-level runs:
        //   (true,  w) on a falling edge  -> the HIGH pulse was w us wide;
        //   (false, p) on a rising edge   -> the full period since the last rising
        //                                    edge was p us (high + low).
        // Reconstruct the two constant-level runs the decoder expects: the high
        // run is w, and the low run of that period is p - w.
        bool level = (e & 0x80000000u) != 0;
        uint32_t dur = e & 0x7FFFFFFFu;

        if(level) {
            // High-pulse width: emit it as a high run and remember it.
            tp->rx_last_high = dur;
            tp->rx_have_high = true;
            rfid_tp_feed_run(tp, true, dur, pkt);
        } else {
            // Full period: the low run is period - last high pulse. A period at or
            // below the last high (or before any high seen — e.g. after a long
            // idle) is treated as an inter-frame gap so the decoder re-hunts.
            if(tp->rx_have_high && dur > tp->rx_last_high) {
                rfid_tp_feed_run(tp, false, dur - tp->rx_last_high, pkt);
            } else {
                rfid_tp_feed_run(tp, false, RFID_MODEM_GAP_US + 1u, pkt);
                tp->rx_have_high = false;
            }
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
            furi_thread_join(tp->rx_worker);
            furi_thread_free(tp->rx_worker);
        }
        if(tp->rx_stream) furi_stream_buffer_free(tp->rx_stream);
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

bool rfid_transport_tag_field_present(void) {
    RfidTransport* tp = rfid_tp;
    return tp && tp->field_present;
}

void rfid_transport_reader_stats(
    uint32_t* events,
    uint32_t* frames,
    uint32_t* valid,
    uint32_t* announce) {
    RfidTransport* tp = rfid_tp;
    bool r = tp && tp->mode == RfidTransportModeReader;
    if(events) *events = r ? tp->rx_events : 0;
    if(frames) *frames = r ? tp->rx_frames : 0;
    if(valid) *valid = r ? tp->rx_valid : 0;
    if(announce) *announce = r ? tp->rx_announce : 0;
}
