#include "nfc_transport.h"
#include "nfc_transport_config.h"
#include "nfc_share.h"

#include <furi.h>

#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>
#include <toolbox/bit_buffer.h>

#define TAG "NfcTransport"

// A full frame (hdr + packet + CRC-A) must fit the firmware's NFC buffer.
_Static_assert(
    NFC_TP_HDR_LEN + NSH_PACKET_MAX + 2u <= NFC_TP_FRAME_PAYLOAD_MAX + 2u,
    "flipper-share packet does not fit into an NFC frame");

typedef struct {
    uint16_t len;
    uint8_t data[NSH_PACKET_MAX];
} NfcTpPacket;

typedef struct {
    NfcTransportMode mode;
    Nfc* nfc;
    NfcListener* listener;
    // Poller role: the poller instance is owned EXCLUSIVELY by the scheduler
    // thread, which duty-cycles the RF field (see nfc_tp_scheduler_thread).
    NfcPoller* poller;
    // Poller role: `scheduler` runs nfc_tp_scheduler_thread (duty-cycles the field).
    // Listener role: `scheduler` runs nfc_tp_listener_watchdog_thread (restarts a
    // wedged card emulation). A transport is one role only, so the field is reused.
    FuriThread* scheduler;
    volatile bool scheduler_stop;
    volatile bool dormant; // set by nfc_transport_stop_field(): drop the field, stay idle
    volatile uint32_t last_success_ms; // last successful exchange (NfcWorker -> scheduler)
    volatile uint32_t last_frame_ms; // listener: last completed RX frame (NfcWorker -> watchdog)
    FuriMessageQueue* tx_queue; // DATA packets waiting to go out (FIFO, paced)
    // Latest pending control packet (ANNOUNCE / REQUEST). Kept OUT of tx_queue
    // and always sent first, so a DATA stream can never starve control traffic —
    // otherwise a receiver that lost lock could never see an ANNOUNCE (nor the
    // sender a REQUEST) and the link would never recover. Latest-wins: a newer
    // control packet supersedes an older unsent one.
    FuriMutex* ctrl_mutex;
    NfcTpPacket ctrl_pkt;
    bool ctrl_valid;
    BitBuffer* tx_frame;
    BitBuffer* rx_frame;
} NfcTransport;

// Owned by the scene lifecycle: init in on_enter, deinit in on_exit, and no
// thread calls nfc_transport_send() during deinit (workers joined first).
static NfcTransport* nfc_tp = NULL;

// Fill tx_frame with the next pending packet, or an empty poll frame.
// Control packets (ANNOUNCE / REQUEST) take priority over queued DATA.
static void nfc_tp_build_tx_frame(NfcTransport* tp) {
    NfcTpPacket pkt;
    bool have = false;

    furi_mutex_acquire(tp->ctrl_mutex, FuriWaitForever);
    if(tp->ctrl_valid) {
        pkt = tp->ctrl_pkt;
        tp->ctrl_valid = false;
        have = true;
    }
    furi_mutex_release(tp->ctrl_mutex);

    if(!have) have = (furi_message_queue_get(tp->tx_queue, &pkt, 0) == FuriStatusOk);

    bit_buffer_reset(tp->tx_frame);
    if(have) {
        bit_buffer_append_byte(tp->tx_frame, NFC_TP_HDR_PKT);
        bit_buffer_append_bytes(tp->tx_frame, pkt.data, pkt.len);
    } else {
        bit_buffer_append_byte(tp->tx_frame, NFC_TP_HDR_POLL);
    }
}

// Deliver the flipper-share packet from a received frame, if any.
static void nfc_tp_dispatch_rx(const uint8_t* data, size_t len) {
    if(len > NFC_TP_HDR_LEN && data[0] == NFC_TP_HDR_PKT) {
        nsh_receive_callback(data + NFC_TP_HDR_LEN, len - NFC_TP_HDR_LEN);
    }
}

// ===== Listener (sender) side ================================================

static NfcCommand nfc_tp_listener_callback(NfcGenericEvent event, void* context);

// Build the emulated card and start answering polls. Also used by the watchdog
// to recover a wedged emulation; re-running nfc_config resets the ST25R3916
// target automaton (GOTO_SENSE + auto-collision-resolution re-enabled).
static void nfc_tp_listener_bringup(NfcTransport* tp) {
    const uint8_t uid[NFC_TP_UID_LEN] = NFC_TP_UID;
    const uint8_t atqa[2] = NFC_TP_ATQA;
    Iso14443_3aData* card = iso14443_3a_alloc();
    iso14443_3a_set_uid(card, uid, NFC_TP_UID_LEN);
    iso14443_3a_set_atqa(card, atqa);
    iso14443_3a_set_sak(card, NFC_TP_SAK);
    tp->listener = nfc_listener_alloc(tp->nfc, NfcProtocolIso14443_3a, (const NfcDeviceData*)card);
    iso14443_3a_free(card); // nfc_listener_alloc stores its own copy
    nfc_listener_start(tp->listener, nfc_tp_listener_callback, tp);
}

static void nfc_tp_listener_teardown(NfcTransport* tp) {
    if(tp->listener) {
        nfc_listener_stop(tp->listener);
        nfc_listener_free(tp->listener);
        tp->listener = NULL;
    }
}

// Runs on the NfcWorker thread. Every valid frame from the poller gets exactly
// one response: the next pending packet or an empty poll frame. Frames that
// don't carry the transport header (a foreign reader) are left unanswered.
static NfcCommand nfc_tp_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    NfcTransport* tp = context;
    Iso14443_3aListenerEvent* e = event.event_data;

    if(e->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        // A completed standard frame proves the emulation is answering — feed
        // the watchdog so it only restarts a genuinely wedged/idle listener.
        tp->last_frame_ms = furi_get_tick();

        const uint8_t* rx = bit_buffer_get_data(e->data->buffer);
        size_t rx_len = bit_buffer_get_size_bytes(e->data->buffer);

        if(rx_len < NFC_TP_HDR_LEN || (rx[0] != NFC_TP_HDR_POLL && rx[0] != NFC_TP_HDR_PKT)) {
            return NfcCommandContinue; // not ours — stay mute, like a real card
        }

        nfc_tp_dispatch_rx(rx, rx_len);

        nfc_tp_build_tx_frame(tp);
        iso14443_crc_append(Iso14443CrcTypeA, tp->tx_frame);
        nfc_listener_tx(tp->nfc, tp->tx_frame);
    }

    return NfcCommandContinue;
}

// ===== Poller (receiver) side ================================================
// Runs on the NfcWorker thread: the stack calls back with a Ready event in a
// loop while the card stays activated; each callback performs one exchange.
//
// EVERY error path here must return NfcCommandReset. Reset is the only command
// that makes the firmware drop the RF field (~100 ms off in
// nfc_worker_poller_reset_handler) — and that field-off is the only thing that
// resets the emulating side. Rationale: after a partial anticollision the
// listener's ST25R3916 is left in the passive-target ACTIVE state with hardware
// auto-collision-resolution disabled, where it answers neither REQA nor WUPA;
// the sole automatic exit is the field-off (EOF) interrupt, which returns it to
// the SENSE/IDLE state. Returning Continue keeps the field energized, so a
// stranded listener stays invisible FOREVER (until the user separates the
// devices far enough to drop the coupled field). Do not downgrade to Continue.
// This mirrors the stock NFC scanner, which cycles the field on every attempt.
static NfcCommand nfc_tp_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    NfcTransport* tp = context;
    Iso14443_3aPoller* poller = event.instance;
    Iso14443_3aPollerEvent* e = event.event_data;

    if(e->type == Iso14443_3aPollerEventTypeError) {
        // Activation failed (no card, or a card stuck mid-anticollision):
        // cycle the field so the next attempt starts from a fresh listener.
        return NfcCommandReset;
    }

    nfc_tp_build_tx_frame(tp);
    Iso14443_3aError err =
        iso14443_3a_poller_send_standard_frame(poller, tp->tx_frame, tp->rx_frame, NFC_TP_FWT_FC);
    if(err != Iso14443_3aErrorNone) {
        FURI_LOG_D(TAG, "exchange failed: %d, re-activating", err);
        return NfcCommandReset;
    }

    nfc_tp_dispatch_rx(bit_buffer_get_data(tp->rx_frame), bit_buffer_get_size_bytes(tp->rx_frame));

    tp->last_success_ms = furi_get_tick(); // link is alive — scheduler keeps the poller

    if(NFC_TP_POLL_PERIOD_MS) furi_delay_ms(NFC_TP_POLL_PERIOD_MS);
    return NfcCommandContinue;
}

// ===== Poller scheduler ======================================================
// Duty-cycles the RF field so the receiver stays cold while there is no peer.
//
// SEARCH: one nfc_poller_detect() probe (field on only ~6 ms: 5 ms guard time +
// one WUPA; detect drops the field itself via its internal nfc_stop) every
// NFC_TP_SEARCH_PERIOD_MS — ~1% field duty vs ~50% of a free-running poller,
// whose failed activation cannot take less than ~105 ms of field time (the
// firmware holds the field through a hardcoded 100 ms delay on the error path).
// The probe is alloc/detect/free each time, mirroring the stock nfc_scanner.
//
// LINKED: the full poller runs continuously (exchange errors fast-retry via
// NfcCommandReset). When no exchange has succeeded for NFC_TP_LINK_GRACE_MS the
// link is considered lost and we fall back to SEARCH — so a transfer separated
// for minutes or hours costs ~1% field duty and still resumes on re-touch (the
// protocol state lives in the engine and is untouched by these transitions).
static int32_t nfc_tp_scheduler_thread(void* context) {
    NfcTransport* tp = context;

    while(!tp->scheduler_stop) {
        // Dormant (transfer finished): field stays off, just idle until deinit.
        if(tp->dormant) {
            furi_delay_ms(50);
            continue;
        }

        // ---- SEARCH ----
        NfcPoller* probe = nfc_poller_alloc(tp->nfc, NfcProtocolIso14443_3a);
        bool found = nfc_poller_detect(probe);
        nfc_poller_free(probe);

        if(!found || tp->dormant || tp->scheduler_stop) {
            // Sleep out the period in slices so Back / stop-field act promptly.
            for(uint32_t s = 0; s < NFC_TP_SEARCH_PERIOD_MS && !tp->scheduler_stop && !tp->dormant;
                s += 50) {
                furi_delay_ms(50);
            }
            continue;
        }

        // ---- LINKED ----
        FURI_LOG_I(TAG, "peer detected, starting poller");
        tp->last_success_ms = furi_get_tick();
        tp->poller = nfc_poller_alloc(tp->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(tp->poller, nfc_tp_poller_callback, tp);

        while(!tp->scheduler_stop && !tp->dormant &&
              (furi_get_tick() - tp->last_success_ms) < NFC_TP_LINK_GRACE_MS) {
            furi_delay_ms(100);
        }

        nfc_poller_stop(tp->poller);
        nfc_poller_free(tp->poller);
        tp->poller = NULL;
        if(!tp->scheduler_stop && !tp->dormant) FURI_LOG_I(TAG, "link stale, back to search");
    }

    return 0;
}

// ===== Listener watchdog =====================================================
// The card emulation (ST25R3916 passive-target automaton) can, rarely, get
// stuck after many rapid field on/off cycles: it stops answering activation, so
// no reader — not even a fresh one — can see it, while our protocol worker keeps
// producing announces. The listener has no self-recovery for this. This thread
// restarts the emulation whenever no standard frame has completed for
// NFC_TP_LISTENER_WATCHDOG_MS: during an active transfer frames arrive
// continuously (so it never fires), and while idle/lost/wedged a restart is free
// and clears the stuck state. Protocol state lives in the engine and is
// untouched, so a transfer resumes after recovery.
static int32_t nfc_tp_listener_watchdog_thread(void* context) {
    NfcTransport* tp = context;

    while(!tp->scheduler_stop) {
        furi_delay_ms(250); // slice so deinit exits promptly
        if(tp->scheduler_stop) break;

        if((furi_get_tick() - tp->last_frame_ms) > NFC_TP_LISTENER_WATCHDOG_MS) {
            FURI_LOG_I(TAG, "listener idle/wedged, restarting emulation");
            nfc_tp_listener_teardown(tp);
            nfc_tp_listener_bringup(tp);
            tp->last_frame_ms = furi_get_tick();
        }
    }

    return 0;
}

// ===== Public API ============================================================

void nfc_transport_init(NfcTransportMode mode) {
    furi_assert(nfc_tp == NULL);

    NfcTransport* tp = malloc(sizeof(NfcTransport));
    memset(tp, 0, sizeof(*tp));
    tp->mode = mode;
    tp->nfc = nfc_alloc();
    tp->tx_queue = furi_message_queue_alloc(NFC_TP_QUEUE_LEN, sizeof(NfcTpPacket));
    tp->ctrl_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    tp->ctrl_valid = false;
    tp->tx_frame = bit_buffer_alloc(NFC_TP_FRAME_PAYLOAD_MAX + 2u);
    tp->rx_frame = bit_buffer_alloc(NFC_TP_FRAME_PAYLOAD_MAX + 2u);

    tp->scheduler_stop = false;
    tp->dormant = false;
    if(mode == NfcTransportModeListener) {
        tp->last_frame_ms = furi_get_tick();
        nfc_tp_listener_bringup(tp);
        // Watchdog restarts the emulation if it ever stops answering.
        tp->scheduler =
            furi_thread_alloc_ex("NfcTpLisWd", 2048, nfc_tp_listener_watchdog_thread, tp);
        furi_thread_start(tp->scheduler);
    } else {
        // The scheduler thread owns the poller and duty-cycles the field.
        tp->scheduler = furi_thread_alloc_ex("NfcTpScheduler", 2048, nfc_tp_scheduler_thread, tp);
        furi_thread_start(tp->scheduler);
    }

    nfc_tp = tp; // publish only when fully started
    FURI_LOG_I(TAG, "started as %s", mode == NfcTransportModeListener ? "listener" : "poller");
}

void nfc_transport_deinit(void) {
    NfcTransport* tp = nfc_tp;
    if(!tp) return;
    nfc_tp = NULL; // sends become no-ops first

    if(tp->scheduler) {
        // Stop the role thread first: the poller scheduler frees its poller on
        // exit; the listener watchdog leaves the listener running for us.
        tp->scheduler_stop = true;
        furi_thread_join(tp->scheduler);
        furi_thread_free(tp->scheduler);
    }
    if(tp->poller) { // defensive: poller scheduler normally leaves this NULL
        nfc_poller_stop(tp->poller);
        nfc_poller_free(tp->poller);
    }
    nfc_tp_listener_teardown(tp);
    nfc_free(tp->nfc);
    bit_buffer_free(tp->tx_frame);
    bit_buffer_free(tp->rx_frame);
    furi_message_queue_free(tp->tx_queue);
    furi_mutex_free(tp->ctrl_mutex);
    free(tp);
    FURI_LOG_I(TAG, "stopped");
}

void nfc_transport_stop_field(void) {
    // Only sets a flag observed by the scheduler thread, so it is safe to call
    // from any thread (unlike nfc_transport_deinit, whose nfc_free must run on
    // the thread that allocated the Nfc instance). The scheduler stops the
    // poller -> RF field off; the instance is fully freed later in deinit.
    NfcTransport* tp = nfc_tp;
    if(tp) tp->dormant = true;
}

void nfc_transport_send(const uint8_t* buf, size_t len) {
    NfcTransport* tp = nfc_tp;
    if(!tp) return; // transport not running — drop, ARQ recovers
    if(len <= NSH_HEADER_LENGTH || len > NSH_PACKET_MAX) {
        FURI_LOG_E(TAG, "bad packet length %zu", len);
        return;
    }

    // ANNOUNCE / REQUEST go to the priority control slot (latest-wins, never
    // dropped by a full DATA queue); DATA goes to the paced FIFO. packet_type is
    // the 3rd header byte.
    if(buf[NSH_HEADER_LENGTH - 1] != NSH_PKT_DATA) {
        furi_mutex_acquire(tp->ctrl_mutex, FuriWaitForever);
        tp->ctrl_pkt.len = len;
        memcpy(tp->ctrl_pkt.data, buf, len);
        tp->ctrl_valid = true;
        furi_mutex_release(tp->ctrl_mutex);
        return;
    }

    NfcTpPacket pkt;
    pkt.len = len;
    memcpy(pkt.data, buf, len);
    if(furi_message_queue_put(tp->tx_queue, &pkt, furi_ms_to_ticks(NFC_TP_SEND_TIMEOUT_MS)) !=
       FuriStatusOk) {
        FURI_LOG_W(TAG, "TX mailbox full, DATA packet dropped");
    }
}
