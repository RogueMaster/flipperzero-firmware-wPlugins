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
#define ISO_DEP_FRAME_MAX    62U // 64-byte FSC minus CRC_A

static void send_frame(
    Dfc* dfc,
    Iso14443_4aListener* iso14443_4a_listener,
    const uint8_t* data,
    size_t length) {
    BitBuffer* radio_buffer = bit_buffer_alloc(DFC_BYTEBUF_MAX);
    bit_buffer_append_bytes(radio_buffer, data, length);
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

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
static void send_response(Dfc* dfc, Iso14443_4aListener* listener, const DfcByteBuf* response) {
    send_frame(
        dfc, listener, dfc_bytebuf_get_data(response), dfc_bytebuf_get_size_bytes(response));
}
#endif

#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
static void
    send_control(Dfc* dfc, Iso14443_4aListener* listener, uint8_t pcb, bool use_cid, uint8_t cid) {
    uint8_t block[2] = {pcb | (use_cid ? ISO14443_4A_CID_MASK : 0), cid};
    send_frame(dfc, listener, block, use_cid ? 2 : 1);
}

static void send_next_i_response(Dfc* dfc, Iso14443_4aListener* listener) {
    const DfcByteBuf* response = dfc->dfc_emulator->tx_buffer;
    const size_t response_len = dfc_bytebuf_get_size_bytes(response);
    const uint8_t* data = dfc_bytebuf_get_data(response);
    const size_t prefix_len = 1 + ((data[0] & ISO14443_4A_CID_MASK) ? 1 : 0);
    if(response_len < prefix_len) return;

    size_t remaining = response_len - dfc->iso_dep_response_offset;
    size_t chunk_len = ISO_DEP_FRAME_MAX - prefix_len;
    if(chunk_len > remaining) chunk_len = remaining;
    bool more = chunk_len < remaining;
    uint8_t* frame = dfc->iso_dep_last_frame;
    memcpy(frame, data, prefix_len);
    frame[0] = (frame[0] & (uint8_t)~0x11) | dfc->iso_dep_picc_block;
    if(more) frame[0] |= 0x10;
    memcpy(frame + prefix_len, data + dfc->iso_dep_response_offset, chunk_len);
    dfc->iso_dep_last_frame_len = prefix_len + chunk_len;
    dfc->iso_dep_response_offset += chunk_len;
    if(!more) dfc->iso_dep_response_offset = 0;
    dfc->iso_dep_last_response_valid = true;
    send_frame(dfc, listener, frame, dfc->iso_dep_last_frame_len);
}
#endif

static void send_i_response(Dfc* dfc, Iso14443_4aListener* listener, DfcByteBuf* response) {
    if(dfc_bytebuf_get_size_bytes(response) == 0) return;
#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
    dfc->iso_dep_picc_block = dfc->iso_dep_expected_pcd_block;
    dfc->iso_dep_expected_pcd_block ^= 1;
    dfc->iso_dep_response_offset = 1 + ((response->data[0] & ISO14443_4A_CID_MASK) ? 1 : 0);
    send_next_i_response(dfc, listener);
#else
    send_response(dfc, listener, response);
#endif
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
    uint8_t scratch_buffer[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t scratch_len = 0;

    switch(iso14443_4a_event->type) {
    case Iso14443_4aListenerEventTypeReceivedData: {
        BitBuffer* rx_buffer = iso14443_4a_event->data->buffer;
        const uint8_t* rx_data = bit_buffer_get_data(rx_buffer);
        size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
        if(rx_len == 0) break;
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        uint8_t offset = 0;
        UNUSED(rx_data);
#else
        uint8_t pcb = rx_data[0];
        uint8_t offset = 1;
        const bool use_cid = (pcb & ISO14443_4A_CID_MASK) != 0;
        if(use_cid) offset++;
        if(rx_len < offset) break;
        const uint8_t cid = use_cid ? rx_data[1] : 0;

        // PPS may only keep the radio at 106 kbit/s. The ATS advertises no
        // higher rate, and the Flipper listener cannot switch rates here.
        if((pcb & 0xF0) == 0xD0) {
            if((rx_len == 2 && rx_data[1] == 0x01) ||
               (rx_len == 3 && rx_data[1] == 0x11 && rx_data[2] == 0x00)) {
                send_control(dfc, iso14443_4a_listener, pcb, false, 0);
            }
            break;
        }

        if((pcb & 0xC0) == 0x80) {
            if((pcb & 0xE6) != 0xA2 || rx_len != offset) break;
            if(dfc->iso_dep_cid_valid != use_cid || (use_cid && cid != dfc->iso_dep_cid)) break;
            if(dfc->iso_dep_last_response_valid && (pcb & 1) == dfc->iso_dep_picc_block) {
                send_frame(
                    dfc,
                    iso14443_4a_listener,
                    dfc->iso_dep_last_frame,
                    dfc->iso_dep_last_frame_len);
            } else if(dfc->iso_dep_response_offset && !(pcb & 0x10)) {
                dfc->iso_dep_picc_block ^= 1;
                send_next_i_response(dfc, iso14443_4a_listener);
                dfc->iso_dep_expected_pcd_block = dfc->iso_dep_picc_block ^ 1;
            } else if(pcb & 0x10) {
                send_control(
                    dfc, iso14443_4a_listener, 0xA2 | dfc->iso_dep_picc_block, use_cid, cid);
            }
            break;
        }
        if((pcb & 0xC0) == 0xC0) {
            if((pcb & (uint8_t)~ISO14443_4A_CID_MASK) == 0xC2 && rx_len == offset &&
               dfc->iso_dep_cid_valid == use_cid && (!use_cid || cid == dfc->iso_dep_cid)) {
                send_frame(dfc, iso14443_4a_listener, rx_data, rx_len);
                dfc_emulator_reset_activation(emulator);
                dfc->iso_dep_last_response_valid = false;
                dfc->iso_dep_expected_pcd_block = 0;
                dfc->iso_dep_picc_block = 1;
                dfc->iso_dep_command_len = 0;
                dfc->iso_dep_response_offset = 0;
                dfc->iso_dep_cid_valid = false;
            }
            break;
        }
        if((pcb & 0xE2) != 0x02 || (pcb & ISO14443_4A_NAD_MASK)) break;
        if(dfc->iso_dep_cid_valid != use_cid || (use_cid && cid != dfc->iso_dep_cid)) {
            if(dfc->iso_dep_last_response_valid || dfc->iso_dep_command_len) break;
            dfc->iso_dep_cid_valid = use_cid;
            dfc->iso_dep_cid = cid;
        }
        if((pcb & 1) != dfc->iso_dep_expected_pcd_block) {
            if(dfc->iso_dep_last_response_valid && !dfc->iso_dep_command_len) {
                send_frame(
                    dfc,
                    iso14443_4a_listener,
                    dfc->iso_dep_last_frame,
                    dfc->iso_dep_last_frame_len);
            } else {
                send_control(
                    dfc,
                    iso14443_4a_listener,
                    dfc->iso_dep_command_len ? (0xA2 | (pcb & 1)) :
                                               (0xB2 | dfc->iso_dep_expected_pcd_block),
                    use_cid,
                    cid);
            }
            break;
        }
        if(dfc->iso_dep_response_offset) {
            send_control(
                dfc, iso14443_4a_listener, 0xB2 | dfc->iso_dep_expected_pcd_block, use_cid, cid);
            break;
        }
#endif

        if(rx_len <= offset) {
#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
            send_control(
                dfc, iso14443_4a_listener, 0xB2 | dfc->iso_dep_expected_pcd_block, use_cid, cid);
#endif
            break;
        }

#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        const uint8_t* apdu = rx_data + offset;
        size_t apdu_len = rx_len - offset;
        if((pcb & 0x10) || dfc->iso_dep_command_len) {
            if(apdu_len > sizeof(dfc->iso_dep_command) - dfc->iso_dep_command_len) {
                dfc->iso_dep_command_len = 0;
                send_control(
                    dfc,
                    iso14443_4a_listener,
                    0xB2 | dfc->iso_dep_expected_pcd_block,
                    use_cid,
                    cid);
                break;
            }
            memcpy(dfc->iso_dep_command + dfc->iso_dep_command_len, apdu, apdu_len);
            dfc->iso_dep_command_len += apdu_len;
            if(pcb & 0x10) {
                dfc->iso_dep_expected_pcd_block ^= 1;
                send_control(dfc, iso14443_4a_listener, 0xA2 | (pcb & 1), use_cid, cid);
                break;
            }
            apdu = dfc->iso_dep_command;
            apdu_len = dfc->iso_dep_command_len;
            dfc->iso_dep_command_len = 0;
        }
        dfc_bytebuf_reset(tx_buffer);
        dfc_bytebuf_append_bytes(tx_buffer, rx_data, offset);
#else
        dfc_bytebuf_reset(tx_buffer);
        const uint8_t* apdu = rx_data + offset;
        size_t apdu_len = rx_len - offset;
#endif
        if(apdu_len > DFC_WORKER_MAX_BUFFER_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, 0x91);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            send_i_response(dfc, iso14443_4a_listener, tx_buffer);
            break;
        }
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
                send_i_response(dfc, iso14443_4a_listener, tx_buffer);
                break;
            }
        }

        if(apdu_len >= 5 && apdu[0] == DFC_ISO7816_CLA_WRAPPER) {
            iso_wrapped = true;
            uint8_t ins = apdu[1];
            uint8_t lc = apdu[4];
            if((size_t)lc > sizeof(scratch_buffer) - 1 || apdu_len < (size_t)5 + lc) {
                dfc_bytebuf_append_byte(tx_buffer, 0x91);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                send_i_response(dfc, iso14443_4a_listener, tx_buffer);
                break;
            }
            scratch_len = 0;
            scratch_buffer[scratch_len++] = ins;
            if(lc > 0) {
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

        send_i_response(dfc, iso14443_4a_listener, tx_buffer);
        break;
    }
    case Iso14443_4aListenerEventTypeHalted:
        dfc_emulator_reset_activation(emulator);
        dfc->iso_dep_last_response_valid = false;
        dfc->iso_dep_expected_pcd_block = 0;
        dfc->iso_dep_picc_block = 1;
        dfc->iso_dep_last_frame_len = 0;
        dfc->iso_dep_command_len = 0;
        dfc->iso_dep_response_offset = 0;
        dfc->iso_dep_cid_valid = false;
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventEmulate);
        FURI_LOG_I(TAG, "Halted");
        break;
    case Iso14443_4aListenerEventTypeFieldOff:
        dfc_emulator_reset_activation(emulator);
        dfc->iso_dep_last_response_valid = false;
        dfc->iso_dep_expected_pcd_block = 0;
        dfc->iso_dep_picc_block = 1;
        dfc->iso_dep_last_frame_len = 0;
        dfc->iso_dep_command_len = 0;
        dfc->iso_dep_response_offset = 0;
        dfc->iso_dep_cid_valid = false;
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventEmulate);
        FURI_LOG_I(TAG, "Field Off");
        break;
    }

    return ret;
}
