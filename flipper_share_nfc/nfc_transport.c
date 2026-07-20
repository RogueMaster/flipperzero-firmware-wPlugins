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
    NfcPoller* poller;
    FuriMessageQueue* tx_queue; // NfcTpPacket items waiting to go out
    BitBuffer* tx_frame;
    BitBuffer* rx_frame;
} NfcTransport;

// Owned by the scene lifecycle: init in on_enter, deinit in on_exit, and no
// thread calls nfc_transport_send() during deinit (workers joined first).
static NfcTransport* nfc_tp = NULL;

// Fill tx_frame with the next pending packet, or an empty poll frame.
static void nfc_tp_build_tx_frame(NfcTransport* tp) {
    NfcTpPacket pkt;
    bit_buffer_reset(tp->tx_frame);
    if(furi_message_queue_get(tp->tx_queue, &pkt, 0) == FuriStatusOk) {
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
// Runs on the NfcWorker thread. Every valid frame from the poller gets exactly
// one response: the next pending packet or an empty poll frame. Frames that
// don't carry the transport header (a foreign reader) are left unanswered.
static NfcCommand nfc_tp_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    NfcTransport* tp = context;
    Iso14443_3aListenerEvent* e = event.event_data;

    if(e->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        const uint8_t* rx = bit_buffer_get_data(e->data->buffer);
        size_t rx_len = bit_buffer_get_size_bytes(e->data->buffer);

        if(rx_len < NFC_TP_HDR_LEN ||
           (rx[0] != NFC_TP_HDR_POLL && rx[0] != NFC_TP_HDR_PKT)) {
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
// Any exchange error forces re-activation (NfcCommandReset), which the stack
// retries indefinitely — this is what makes the link resume after separation.
static NfcCommand nfc_tp_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    NfcTransport* tp = context;
    Iso14443_3aPoller* poller = event.instance;
    Iso14443_3aPollerEvent* e = event.event_data;

    if(e->type == Iso14443_3aPollerEventTypeError) {
        return NfcCommandContinue; // no card in field yet — keep trying
    }

    nfc_tp_build_tx_frame(tp);
    Iso14443_3aError err = iso14443_3a_poller_send_standard_frame(
        poller, tp->tx_frame, tp->rx_frame, NFC_TP_FWT_FC);
    if(err != Iso14443_3aErrorNone) {
        // Reset (re-activate) on ANY error, not just field-off. Once the card
        // leaves the field the poller stays "activated" but every exchange
        // fails; only re-activation recovers it, so a bare timeout MUST reset
        // to resume the transfer when the devices are re-touched. This costs a
        // ~100 ms field cycle per glitch under marginal coupling — an accepted
        // trade for reliable resume. Do not downgrade to Continue.
        FURI_LOG_D(TAG, "exchange failed: %d, re-activating", err);
        return NfcCommandReset;
    }

    nfc_tp_dispatch_rx(
        bit_buffer_get_data(tp->rx_frame), bit_buffer_get_size_bytes(tp->rx_frame));

    if(NFC_TP_POLL_PERIOD_MS) furi_delay_ms(NFC_TP_POLL_PERIOD_MS);
    return NfcCommandContinue;
}

// ===== Public API ============================================================

void nfc_transport_init(NfcTransportMode mode) {
    furi_assert(nfc_tp == NULL);

    NfcTransport* tp = malloc(sizeof(NfcTransport));
    memset(tp, 0, sizeof(*tp));
    tp->mode = mode;
    tp->nfc = nfc_alloc();
    tp->tx_queue = furi_message_queue_alloc(NFC_TP_QUEUE_LEN, sizeof(NfcTpPacket));
    tp->tx_frame = bit_buffer_alloc(NFC_TP_FRAME_PAYLOAD_MAX + 2u);
    tp->rx_frame = bit_buffer_alloc(NFC_TP_FRAME_PAYLOAD_MAX + 2u);

    if(mode == NfcTransportModeListener) {
        const uint8_t uid[NFC_TP_UID_LEN] = NFC_TP_UID;
        const uint8_t atqa[2] = NFC_TP_ATQA;
        Iso14443_3aData* card = iso14443_3a_alloc();
        iso14443_3a_set_uid(card, uid, NFC_TP_UID_LEN);
        iso14443_3a_set_atqa(card, atqa);
        iso14443_3a_set_sak(card, NFC_TP_SAK);
        tp->listener =
            nfc_listener_alloc(tp->nfc, NfcProtocolIso14443_3a, (const NfcDeviceData*)card);
        iso14443_3a_free(card); // nfc_listener_alloc stores its own copy
        nfc_listener_start(tp->listener, nfc_tp_listener_callback, tp);
    } else {
        tp->poller = nfc_poller_alloc(tp->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(tp->poller, nfc_tp_poller_callback, tp);
    }

    nfc_tp = tp; // publish only when fully started
    FURI_LOG_I(TAG, "started as %s", mode == NfcTransportModeListener ? "listener" : "poller");
}

void nfc_transport_deinit(void) {
    NfcTransport* tp = nfc_tp;
    if(!tp) return;
    nfc_tp = NULL; // sends become no-ops first

    if(tp->listener) {
        nfc_listener_stop(tp->listener);
        nfc_listener_free(tp->listener);
    }
    if(tp->poller) {
        nfc_poller_stop(tp->poller);
        nfc_poller_free(tp->poller);
    }
    nfc_free(tp->nfc);
    bit_buffer_free(tp->tx_frame);
    bit_buffer_free(tp->rx_frame);
    furi_message_queue_free(tp->tx_queue);
    free(tp);
    FURI_LOG_I(TAG, "stopped");
}

void nfc_transport_send(const uint8_t* buf, size_t len) {
    NfcTransport* tp = nfc_tp;
    if(!tp) return; // transport not running — drop, ARQ recovers
    if(len == 0 || len > NSH_PACKET_MAX) {
        FURI_LOG_E(TAG, "bad packet length %zu", len);
        return;
    }

    NfcTpPacket pkt;
    pkt.len = len;
    memcpy(pkt.data, buf, len);
    if(furi_message_queue_put(tp->tx_queue, &pkt, furi_ms_to_ticks(NFC_TP_SEND_TIMEOUT_MS)) !=
       FuriStatusOk) {
        FURI_LOG_W(TAG, "TX mailbox full, packet dropped");
    }
}
