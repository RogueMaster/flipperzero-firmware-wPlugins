/* Flipper radio transport for the engine.
 *
 * Strips ISO-DEP framing, hands the engine a bare command, and returns the
 * response the engine wrote. The engine's buffer and the radio's buffer are
 * different types on purpose; they meet only in send_response.
 */
#include <lib/nfc/protocols/nfc_generic_event.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <lib/nfc/helpers/iso14443_crc.h>

#include "dfc_emulator_i.h"
#include "dfc_i.h"

#define TAG                  DFC_EMULATOR_TAG
#define ISO14443_4A_CID_MASK DFC_ISO14443_4A_CID_MASK
#define ISO14443_4A_NAD_MASK DFC_ISO14443_4A_NAD_MASK

static void
    send_response(Dfc* dfc, Iso14443_4aListener* iso14443_4a_listener, DfcByteBuf* tx_buffer) {
    // The engine wrote into its own buffer. Copy it into the radio's buffer
    // here, which is the only place the two representations meet.
    BitBuffer* radio_buffer = bit_buffer_alloc(DFC_BYTEBUF_MAX);
    bit_buffer_append_bytes(
        radio_buffer, dfc_bytebuf_get_data(tx_buffer), dfc_bytebuf_get_size_bytes(tx_buffer));
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
    UNUSED(dfc);
    Iso14443_4aError error = iso14443_4a_listener_send_block(iso14443_4a_listener, radio_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "Tx error: %d", error);
    }
#else
    UNUSED(iso14443_4a_listener);
    iso14443_crc_append(Iso14443CrcTypeA, radio_buffer);
    NfcError error = nfc_listener_tx(dfc->nfc, radio_buffer);
    if(error != NfcErrorNone) {
        FURI_LOG_W(TAG, "Tx error: %d", error);
    }
#endif
    bit_buffer_free(radio_buffer);
}

NfcCommand dfc_worker_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.event_data);
    Dfc* dfc = context;
    DfcEmulator* emulator = dfc->dfc_emulator;

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aListenerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aListener* iso14443_4a_listener = event.instance;

    DfcByteBuf* tx_buffer = emulator->tx_buffer;
    dfc_bytebuf_reset(tx_buffer);
    uint8_t scratch_buffer[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t scratch_len = 0;

    switch(iso14443_4a_event->type) {
    case Iso14443_4aListenerEventTypeReceivedData: {
        BitBuffer* rx_buffer = iso14443_4a_event->data->buffer;
        const uint8_t* rx_data = bit_buffer_get_data(rx_buffer);
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        uint8_t offset = 0;
        UNUSED(rx_data);
#else
        uint8_t pcb = rx_data[0];
        uint8_t offset = 1;
        if(pcb & ISO14443_4A_CID_MASK) offset++;
        if((pcb & 0xC0) == 0x00 && (pcb & ISO14443_4A_NAD_MASK)) offset++;
#endif

        if(rx_len == offset) {
#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
            dfc_bytebuf_append_bytes(tx_buffer, rx_data, offset);
            send_response(dfc, iso14443_4a_listener, tx_buffer);
#endif
            break;
        }

#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        if((pcb & 0xC0) == 0xC0) {
            dfc_bytebuf_append_byte(tx_buffer, pcb);
            if((pcb & 0x30) == 0x30 && rx_len > offset) {
                dfc_bytebuf_append_byte(tx_buffer, rx_data[offset]);
            }
            send_response(dfc, iso14443_4a_listener, tx_buffer);
            break;
        }
        if((pcb & 0xC0) == 0x80) {
            dfc_bytebuf_append_byte(tx_buffer, pcb);
            send_response(dfc, iso14443_4a_listener, tx_buffer);
            break;
        }

        dfc_bytebuf_append_bytes(tx_buffer, rx_data, offset);
#endif

        const uint8_t* apdu = rx_data + offset;
        size_t apdu_len = rx_len - offset;
        const uint8_t* native_apdu = apdu;
        size_t native_apdu_len = apdu_len;
        bool iso_wrapped = false;

        if(apdu_len >= 4 && apdu[0] == DFC_ISO7816_CLA_STANDARD) {
            bool app_selected = false;
            if(dfc_emulator_handle_iso7816_select(
                   emulator, apdu, apdu_len, tx_buffer, offset, &app_selected)) {
                if(app_selected && dfc) {
                    view_dispatcher_send_custom_event(
                        dfc->view_dispatcher, DfcCustomEventAppSelected);
                }
                send_response(dfc, iso14443_4a_listener, tx_buffer);
                break;
            }
        }

        if(apdu_len >= 5 && apdu[0] == DFC_ISO7816_CLA_WRAPPER) {
            iso_wrapped = true;
            uint8_t ins = apdu[1];
            uint8_t lc = apdu[4];
            scratch_len = 0;
            scratch_buffer[scratch_len++] = ins;
            if(lc > 0 && apdu_len >= (size_t)(5 + lc)) {
                memcpy(&scratch_buffer[scratch_len], &apdu[5], lc);
                scratch_len += lc;
            }

            native_apdu = scratch_buffer;
            native_apdu_len = scratch_len;
        }

        if(!dfc_emulator_handle_command(emulator, native_apdu, native_apdu_len, tx_buffer, dfc)) {
            view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventEmulate);
        }

        if(iso_wrapped) {
            size_t tx_len = dfc_bytebuf_get_size_bytes(tx_buffer);
            if(tx_len > offset) {
                const uint8_t* tx_data = dfc_bytebuf_get_data(tx_buffer);
                scratch_len = 0;
                if(dfc_wrap_native_response_as_iso7816(
                       tx_data,
                       tx_len,
                       offset,
                       scratch_buffer,
                       sizeof(scratch_buffer),
                       &scratch_len)) {
                    dfc_bytebuf_reset(tx_buffer);
                    dfc_bytebuf_append_bytes(tx_buffer, scratch_buffer, scratch_len);
                }
            }
        }

        send_response(dfc, iso14443_4a_listener, tx_buffer);
        break;
    }
    case Iso14443_4aListenerEventTypeHalted:
        dfc_emulator_reset_session(emulator);
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventEmulate);
        FURI_LOG_I(TAG, "Halted");
        break;
    case Iso14443_4aListenerEventTypeFieldOff:
        dfc_emulator_reset_session(emulator);
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventEmulate);
        FURI_LOG_I(TAG, "Field Off");
        break;
    }

    return ret;
}
