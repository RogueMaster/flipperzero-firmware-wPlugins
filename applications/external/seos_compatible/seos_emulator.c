#include "seos_emulator_i.h"

#include "seos_protocol.h"
#include "seos_sm_command.h"
#include "seos_sm_event_ui.h"
#include "seos_iso14443_4.h"

#define TAG "SeosEmulator"

#define DESFIRE_CLA 0x90

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t MOBILE_SEOS_ADMIN_CARD[] =
    {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x2d, 0x00, 0x01, 0x01};
static uint8_t OPERATION_SELECTOR[] = {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x2f, 0x00, 0x01, 0x01};
static uint8_t OPERATION_SELECTOR_POST_RESET[] =
    {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x31, 0x00, 0x01, 0x01};
static uint8_t DESFIRE_ISO_AID[] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};

SeosEmulator* seos_emulator_alloc(SeosCredential* credential) {
    SeosEmulator* seos_emulator = malloc(sizeof(SeosEmulator));
    memset(seos_emulator, 0, sizeof(SeosEmulator));

    // Using DES for greater compatibilty
    seos_emulator->params.cipher = TWO_KEY_3DES_CBC_MODE;
    seos_emulator->params.hash = SHA1;

    seos_worker_random_nonce(seos_emulator->params.rndICC, sizeof(seos_emulator->params.rndICC));
    seos_worker_random_nonce(seos_emulator->params.rNonce, sizeof(seos_emulator->params.rNonce));
    seos_emulator->credential = credential;

    seos_emulator->secure_messaging = NULL;

    seos_emulator->tx_buffer = bit_buffer_alloc(SEOS_WORKER_MAX_BUFFER_SIZE);

    return seos_emulator;
}

void seos_emulator_free(SeosEmulator* seos_emulator) {
    furi_assert(seos_emulator);

    if(seos_emulator->secure_messaging) {
        secure_messaging_free(seos_emulator->secure_messaging);
    }

    bit_buffer_free(seos_emulator->tx_buffer);
    free(seos_emulator);
}

/* Where the command starts in a received frame, and how long it is.
 *
 * Newer firmware strips the block header before handing the frame over, so
 * there is nothing to skip; otherwise the header is ours to read past. */
static bool emulator_apdu_bounds(const BitBuffer* rx_buffer, size_t* offset, size_t* apdu_len) {
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    size_t len = bit_buffer_get_size_bytes(rx_buffer);

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
    UNUSED(data);
    if(len == 0) return false;
    *offset = 0;
    *apdu_len = len;
    return true;
#else
    return seos_iso14443_4_apdu_bounds(data, len, offset, apdu_len);
#endif
}

/* Bytes of response this card may put in one frame.
 *
 * Taken from the frame size it advertises when selected, rather than a fixed
 * budget that may be smaller or larger than what was agreed. */
static size_t emulator_frame_budget(Seos* seos) {
    const Iso14443_4aData* data = nfc_device_get_data(seos->nfc_device, NfcProtocolIso14443_4a);
    if(!data) return SEOS_SM_MAX_FRAME;

    size_t budget = seos_iso14443_4_payload_budget(iso14443_4a_get_frame_size_max(data));
    return budget > 0 ? budget : SEOS_SM_MAX_FRAME;
}

NfcCommand seos_worker_listener_inspect_reader(Seos* seos) {
    SeosEmulator* seos_emulator = seos->seos_emulator;
    BitBuffer* tx_buffer = seos_emulator->tx_buffer;
    NfcCommand ret = NfcCommandContinue;

    size_t offset = 0;
    size_t apdu_len = 0;
    if(!emulator_apdu_bounds(seos_emulator->rx_buffer, &offset, &apdu_len)) {
        FURI_LOG_I(TAG, "Frame carries no command");
        return ret;
    }
    const uint8_t* apdu = bit_buffer_get_data(seos_emulator->rx_buffer) + offset;

    const uint8_t* aid = NULL;
    size_t aid_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        if((aid_len == sizeof(OPERATION_SELECTOR) &&
            memcmp(aid, OPERATION_SELECTOR, aid_len) == 0)) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR");
            uint8_t enableInspection[] = {
                0x6f, 0x08, 0x85, 0x06, 0x02, 0x01, 0x40, 0x02, 0x01, 0x00};

            bit_buffer_append_bytes(tx_buffer, enableInspection, sizeof(enableInspection));
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAIDSelected);
        } else {
            FURI_LOG_I(TAG, "Inspect mode: reject other AID");
            bit_buffer_append_bytes(
                tx_buffer, (uint8_t*)SEOS_SW_FILE_NOT_FOUND, sizeof(SEOS_SW_FILE_NOT_FOUND));
        }
    } else if(apdu[0] == DESFIRE_CLA) {
        FURI_LOG_I(TAG, "Desfire command received: ignore");
    } else if(apdu_len > 2) {
        FURI_LOG_I(TAG, "NFC stop; %d bytes", bit_buffer_get_size_bytes(seos_emulator->rx_buffer));
        ret = NfcCommandStop;
    }

    return ret;
}

/* Set when the branch that ran produced a complete response, status word and
 * all, so the caller must not add one. */

NfcCommand seos_worker_listener_process_message(Seos* seos) {
    SeosEmulator* seos_emulator = seos->seos_emulator;
    seos_emulator->response_complete = false;
    BitBuffer* tx_buffer = seos_emulator->tx_buffer;
    NfcCommand ret = NfcCommandContinue;

    size_t offset = 0;
    size_t apdu_len = 0;
    if(!emulator_apdu_bounds(seos_emulator->rx_buffer, &offset, &apdu_len)) {
        FURI_LOG_I(TAG, "Frame carries no command");
        return ret;
    }
    const uint8_t* apdu = bit_buffer_get_data(seos_emulator->rx_buffer) + offset;

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    const uint8_t* oid_list = NULL;
    size_t oid_list_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        seos_emulator->credential->use_hardcoded = false;
        if((aid_len == sizeof(standard_seos_aid) &&
            memcmp(aid, standard_seos_aid, aid_len) == 0)) {
            seos_emulator_select_aid(seos_emulator->tx_buffer, aid, aid_len);
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAIDSelected);
        } else if((aid_len == sizeof(OPERATION_SELECTOR_POST_RESET) &&
                   memcmp(aid, OPERATION_SELECTOR_POST_RESET, aid_len) == 0)) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR_POST_RESET");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer,
                (uint8_t*)SEOS_SW_FILE_NOT_FOUND,
                sizeof(SEOS_SW_FILE_NOT_FOUND));
        } else if((aid_len == sizeof(OPERATION_SELECTOR) &&
                   memcmp(aid, OPERATION_SELECTOR, aid_len) == 0)) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer,
                (uint8_t*)SEOS_SW_FILE_NOT_FOUND,
                sizeof(SEOS_SW_FILE_NOT_FOUND));
        } else if((aid_len == sizeof(MOBILE_SEOS_ADMIN_CARD) &&
                   memcmp(aid, MOBILE_SEOS_ADMIN_CARD, aid_len) == 0)) {
            FURI_LOG_I(TAG, "MOBILE_SEOS_ADMIN_CARD");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer,
                (uint8_t*)SEOS_SW_FILE_NOT_FOUND,
                sizeof(SEOS_SW_FILE_NOT_FOUND));
        } else if((aid_len == sizeof(DESFIRE_ISO_AID) &&
                   memcmp(aid, DESFIRE_ISO_AID, aid_len) == 0)) {
            FURI_LOG_I(TAG, "DESFIRE_ISO_AID");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer,
                (uint8_t*)SEOS_SW_FILE_NOT_FOUND,
                sizeof(SEOS_SW_FILE_NOT_FOUND));
        } else {
            seos_log_bitbuffer(TAG, "Reject unknown AID", seos_emulator->rx_buffer);
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer,
                (uint8_t*)SEOS_SW_FILE_NOT_FOUND,
                sizeof(SEOS_SW_FILE_NOT_FOUND));
        }
    } else if(seos_parse_select_adf(apdu, apdu_len, &oid_list, &oid_list_len)) {
        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_emulator->params,
               seos_emulator->credential,
               seos_emulator->tx_buffer)) {
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventADFMatched);
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
            seos_emulator_shill_select_adf(seos_emulator->tx_buffer);
            seos_emulator->response_complete = true;
        }
    } else if(seos_is_general_authenticate_1(apdu, apdu_len)) {
        seos_emulator_general_authenticate_1(seos_emulator->tx_buffer, seos_emulator->params);
    } else if(seos_is_general_authenticate_2(apdu, apdu_len)) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               apdu_len,
               seos_emulator->credential,
               &seos_emulator->params,
               seos_emulator->tx_buffer)) {
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
            seos_emulator_shill_authenticate(seos_emulator->tx_buffer);
            seos_emulator->response_complete = true;
            return ret;
        }
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAuthenticated);

        /* Replacing a session without freeing it leaks the old one, keys
         * included, which is what freeing it clears. */
        if(seos_emulator->secure_messaging) {
            secure_messaging_free(seos_emulator->secure_messaging);
        }
        seos_emulator->secure_messaging = secure_messaging_alloc(&seos_emulator->params);
        if(!seos_emulator->secure_messaging) {
            FURI_LOG_W(TAG, "Could not start secure messaging");
        }
    } else if(
        apdu_len >= SEOS_GET_RESPONSE_LEN &&
        memcmp(apdu, SEOS_GET_RESPONSE, SEOS_GET_RESPONSE_LEN - 1) == 0) {
        seos_emulator->response_complete = true;
        if(seos_emulator->secure_messaging) {
            seos_sm_command_get_response(
                seos_emulator->secure_messaging,
                emulator_frame_budget(seos),
                apdu[SEOS_GET_RESPONSE_LEN - 1],
                tx_buffer);
        } else {
            seos_sm_append_status(tx_buffer, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else if(seos_sm_command_matches(apdu, apdu_len)) {
        seos_emulator->response_complete = true;
        if(seos_emulator->secure_messaging) {
            if(!seos_sm_command_handle(
                   seos_emulator->secure_messaging,
                   seos_emulator->credential,
                   apdu,
                   apdu_len,
                   emulator_frame_budget(seos),
                   tx_buffer,
                   seos_sm_event_to_view_dispatcher,
                   seos)) {
                secure_messaging_free(seos_emulator->secure_messaging);
                seos_emulator->secure_messaging = NULL;
            }
        } else {
            seos_sm_append_status(tx_buffer, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else {
        // I'm trying to find a good place to re-assert that we're emulating so we don't get stuck on a previous UI screen when we emulate repeatedly
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventEmulate);
    }

    return ret;
}

NfcCommand seos_worker_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.event_data);
    Seos* seos = context;
    SeosEmulator* seos_emulator = seos->seos_emulator;

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aListenerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aListener* iso14443_4a_listener = event.instance;

    BitBuffer* tx_buffer = seos_emulator->tx_buffer;
    bit_buffer_reset(tx_buffer);

    switch(iso14443_4a_event->type) {
    case Iso14443_4aListenerEventTypeReceivedData:
        seos_emulator->rx_buffer = iso14443_4a_event->data->buffer;
        const uint8_t* rx_data = bit_buffer_get_data(seos_emulator->rx_buffer);
        size_t rx_len = bit_buffer_get_size_bytes(seos_emulator->rx_buffer);

#if !__has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        /* A block that is not carrying a command is answered here rather than
         * passed on: an R-block asks for the last answer again, and an S-block
         * ends the exchange. Neither is an APDU. */
        if(rx_len > 0) {
            uint8_t rx_pcb = rx_data[0];
            SeosIso14443_4BlockType block = seos_iso14443_4_classify(rx_pcb);

            if(block == SeosIso14443_4BlockS) {
                /* Answered with the same block, and the session is over. */
                bit_buffer_append_byte(tx_buffer, seos_iso14443_4_response_pcb(rx_pcb, false));
                iso14443_crc_append(Iso14443CrcTypeA, tx_buffer);
                nfc_listener_tx(seos->nfc, tx_buffer);
                if(seos_iso14443_4_is_deselect(rx_pcb)) {
                    FURI_LOG_I(TAG, "Deselected");
                }
                break;
            }

            if(block == SeosIso14443_4BlockR) {
                /* Nothing is held to resend, so this is answered with an empty
                 * I-block rather than left unanswered. */
                FURI_LOG_I(TAG, "Supervisory block with nothing to resend");
                bit_buffer_append_byte(tx_buffer, seos_iso14443_4_response_pcb(rx_pcb, false));
                bit_buffer_append_bytes(tx_buffer, SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
                iso14443_crc_append(Iso14443CrcTypeA, tx_buffer);
                nfc_listener_tx(seos->nfc, tx_buffer);
                break;
            }

            if(seos_iso14443_4_is_chaining(rx_pcb)) {
                /* More of this command is still to come. The pieces are not
                 * assembled -- nothing this serves needs a chained command --
                 * but the other end is told so rather than left waiting for a
                 * reply that never arrives. */
                FURI_LOG_W(TAG, "Chained command not handled");
                uint8_t nak_pcb = seos_iso14443_4_nak_pcb(rx_pcb);
                bit_buffer_append_byte(tx_buffer, nak_pcb);
                /* The identifier is carried back whenever the bit says it is
                 * there; a block that claims one and omits it is malformed. */
                if((nak_pcb & SEOS_ISO14443_4_PCB_CID) && rx_len > 1) {
                    bit_buffer_append_byte(tx_buffer, rx_data[1]);
                }
                iso14443_crc_append(Iso14443CrcTypeA, tx_buffer);
                nfc_listener_tx(seos->nfc, tx_buffer);
                break;
            }
        }
#endif
        if(rx_data[0]) {
            // DONT DO ANYTHING JUST STFU WITH THE ERRORS
        }
        size_t offset = 0;
        size_t apdu_len = 0;
        if(!emulator_apdu_bounds(seos_emulator->rx_buffer, &offset, &apdu_len)) {
            FURI_LOG_I(TAG, "No contents in frame");
            break;
        }

        seos_log_bitbuffer(TAG, "NFC received", seos_emulator->rx_buffer);

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        // With PR #4242 ISO14443-4A response PCB is handled by firmware and not necessary in tx buffer
        UNUSED(rx_len);
#else
        /* Built rather than echoed: the block number has to match what was
         * sent, and everything else in the byte is ours to decide. */
        bit_buffer_append_byte(tx_buffer, seos_iso14443_4_response_pcb(rx_data[0], false));
        if(rx_data[0] & SEOS_ISO14443_4_PCB_CID) {
            bit_buffer_append_byte(tx_buffer, rx_data[1]);
        }
#endif

        if(seos->flow_mode == FLOW_CRED) {
            ret = seos_worker_listener_process_message(seos);
        } else if(seos->flow_mode == FLOW_INSPECT) {
            ret = seos_worker_listener_inspect_reader(seos);
        }

        /* The plain command handlers append their data and leave the status
         * word to us. The secure messaging ones answer in full, including a
         * chaining or error status word that must not be written over. */
        if(!seos_emulator->response_complete &&
           bit_buffer_get_size_bytes(seos_emulator->tx_buffer) > sizeof(uint16_t)) {
            uint8_t* statusword = (uint8_t*)bit_buffer_get_data(tx_buffer) +
                                  bit_buffer_get_size_bytes(tx_buffer) - sizeof(uint16_t);
            bool has_status =
                memcmp(SEOS_SW_SUCCESS, statusword, sizeof(SEOS_SW_SUCCESS)) == 0 ||
                memcmp(SEOS_SW_FILE_NOT_FOUND, statusword, sizeof(SEOS_SW_FILE_NOT_FOUND)) == 0;
            if(!has_status) {
                bit_buffer_append_bytes(tx_buffer, SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
            }
        }

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        // With PR #4242 ISO14443-4A response CRC is handled by firmware and not necessary in tx buffer
#else
        iso14443_crc_append(Iso14443CrcTypeA, tx_buffer);
#endif

        seos_log_bitbuffer(TAG, "NFC transmit", seos_emulator->tx_buffer);

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        // With PR #4242 ISO14443-4A use the public API that handles response PCB and CRC
        Iso14443_4aError error = iso14443_4a_listener_send_block(iso14443_4a_listener, tx_buffer);
        if(error != Iso14443_4aErrorNone) {
#else
        UNUSED(iso14443_4a_listener);
        NfcError error = nfc_listener_tx(seos->nfc, tx_buffer);
        if(error != NfcErrorNone) {
#endif
            FURI_LOG_W(TAG, "Tx error: %d", error);
            break;
        }
        break;
    case Iso14443_4aListenerEventTypeHalted:
        FURI_LOG_I(TAG, "Halted");
        break;
    case Iso14443_4aListenerEventTypeFieldOff:
        FURI_LOG_I(TAG, "Field Off");
        break;
    }

    if(ret == NfcCommandStop) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerError);
    }
    return ret;
}
