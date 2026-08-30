#include "seos_characteristic_i.h"

#include "seos_sm_command.h"
#include "../seos_sm_event_ui.h"
#include "../ble_shared/seos_ble_framing.h"

#define TAG "SeosCharacteristic"

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t cd02[] = {0xcd, 0x02};

static uint8_t ga1_response[] = {0x7c, 0x0a, 0x81, 0x08};

// Emulation

SeosCharacteristic* seos_characteristic_alloc(Seos* seos) {
    SeosCharacteristic* seos_characteristic = malloc(sizeof(SeosCharacteristic));
    memset(seos_characteristic, 0, sizeof(SeosCharacteristic));
    seos_characteristic->seos = seos;
    seos_characteristic->credential = seos->credential;

    seos_characteristic->phase = SELECT_AID;
    seos_characteristic->secure_messaging = NULL;

    seos_characteristic->params.key_no = 1;
    seos_worker_random_nonce(
        seos_characteristic->params.cNonce, sizeof(seos_characteristic->params.cNonce));
    seos_worker_random_nonce(
        seos_characteristic->params.UID, sizeof(seos_characteristic->params.UID));

    seos_characteristic->seos_att = seos_att_alloc(seos);

    seos_att_set_on_subscribe_callback(
        seos_characteristic->seos_att, seos_characteristic_on_subscribe, seos_characteristic);

    seos_att_set_write_request_callback(
        seos_characteristic->seos_att, seos_characteristic_write_request, seos_characteristic);

    seos_characteristic->assembled = bit_buffer_alloc(SEOS_SM_RESPONSE_MAX);
    seos_characteristic->rx_buffer = bit_buffer_alloc(128); // TODO: MTU

    return seos_characteristic;
}

void seos_characteristic_free(SeosCharacteristic* seos_characteristic) {
    furi_assert(seos_characteristic);
    seos_att_free(seos_characteristic->seos_att);
    bit_buffer_free(seos_characteristic->rx_buffer);
    bit_buffer_free(seos_characteristic->assembled);

    if(seos_characteristic->secure_messaging) {
        secure_messaging_free(seos_characteristic->secure_messaging);
    }

    free(seos_characteristic);
}

void seos_characteristic_start(SeosCharacteristic* seos_characteristic, FlowMode mode) {
    seos_characteristic->flow_mode = mode;
    if(seos_characteristic->flow_mode == FLOW_CRED) {
        seos_characteristic->params.key_no = 0;
        seos_characteristic->params.cipher = TWO_KEY_3DES_CBC_MODE;
        seos_characteristic->params.hash = SHA1;

        seos_worker_random_nonce(
            seos_characteristic->params.rndICC, sizeof(seos_characteristic->params.rndICC));
        seos_worker_random_nonce(
            seos_characteristic->params.rNonce, sizeof(seos_characteristic->params.rNonce));
        memset(seos_characteristic->params.UID, 0x00, sizeof(seos_characteristic->params.UID));
        memset(
            seos_characteristic->params.cNonce, 0x00, sizeof(seos_characteristic->params.cNonce));
    }
    seos_att_start(seos_characteristic->seos_att, BLE_PERIPHERAL, mode);
}

void seos_characteristic_stop(SeosCharacteristic* seos_characteristic) {
    seos_att_stop(seos_characteristic->seos_att);
}

/* Every message carries one byte of framing ahead of the command. A message
 * with nothing after that byte carries no command, and counting from it would
 * run the length backwards past zero. */
#define BLE_FRAMING_LEN 1

static bool
    ble_apdu_bounds(const BitBuffer* attribute_value, const uint8_t** apdu, size_t* apdu_len) {
    size_t len = bit_buffer_get_size_bytes(attribute_value);
    if(len <= BLE_FRAMING_LEN) return false;

    *apdu = bit_buffer_get_data(attribute_value) + BLE_FRAMING_LEN;
    *apdu_len = len - BLE_FRAMING_LEN;
    return true;
}

void seos_characteristic_reader_flow(
    SeosCharacteristic* seos_characteristic,
    BitBuffer* attribute_value,
    BitBuffer* payload) {
    const uint8_t* card_cryptogram = NULL;
    size_t card_cryptogram_len = 0;

    const uint8_t* rx_data = NULL;
    size_t rx_len = 0;
    if(!ble_apdu_bounds(attribute_value, &rx_data, &rx_len)) {
        FURI_LOG_I(TAG, "Message carries no response");
        return;
    }

    /* The select answer names the application four bytes into the response. */
    const size_t select_aid_offset = 4;
    if(rx_len >= select_aid_offset + sizeof(standard_seos_aid) &&
       memcmp(rx_data + select_aid_offset, standard_seos_aid, sizeof(standard_seos_aid)) ==
           0) { // response to select
        FURI_LOG_I(TAG, "Select ADF");
        uint8_t select_adf_header[] = {
            0x80, 0xa5, 0x04, 0x00, (uint8_t)SEOS_ADF_OID_LEN + 2, 0x06, (uint8_t)SEOS_ADF_OID_LEN};

        bit_buffer_append_bytes(payload, select_adf_header, sizeof(select_adf_header));
        bit_buffer_append_bytes(payload, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
        seos_characteristic->phase = SELECT_ADF;
    } else if(rx_len >= sizeof(cd02) && memcmp(rx_data, cd02, sizeof(cd02)) == 0) {
        if(seos_reader_select_adf_response(
               attribute_value, 1, seos_characteristic->credential, &seos_characteristic->params)) {
            // Craft response
            uint8_t general_authenticate_1[SEOS_GENERAL_AUTHENTICATE_1_LEN];
            seos_build_general_authenticate_1(
                seos_characteristic->params.key_no, general_authenticate_1);
            bit_buffer_append_bytes(
                payload, general_authenticate_1, sizeof(general_authenticate_1));
            seos_characteristic->phase = GENERAL_AUTHENTICATION_1;
        }
    } else if(
        rx_len >= sizeof(ga1_response) &&
        memcmp(rx_data, ga1_response, sizeof(ga1_response)) == 0 &&
        seos_parse_ga1_response(
            rx_data,
            rx_len,
            seos_characteristic->params.rndICC,
            sizeof(seos_characteristic->params.rndICC))) {
        // Craft response
        uint8_t cryptogram[32 + 8];
        memset(cryptogram, 0, sizeof(cryptogram));
        seos_reader_generate_cryptogram(
            seos_characteristic->credential, &seos_characteristic->params, cryptogram);

        uint8_t ga_header[] = {
            0x00,
            0x87,
            0x00,
            seos_characteristic->params.key_no,
            sizeof(cryptogram) + 4,
            0x7c,
            sizeof(cryptogram) + 2,
            0x82,
            sizeof(cryptogram)};

        bit_buffer_append_bytes(payload, ga_header, sizeof(ga_header));
        bit_buffer_append_bytes(payload, cryptogram, sizeof(cryptogram));

        seos_characteristic->phase = GENERAL_AUTHENTICATION_2;
    } else if(seos_parse_ga2_response(rx_data, rx_len, &card_cryptogram, &card_cryptogram_len)) {
        /* Nothing past here happens unless the card proved it holds the key:
         * a session built on an unverified cryptogram is not a session. */
        if(card_cryptogram_len != SEOS_CARD_CRYPTOGRAM_LEN) {
            FURI_LOG_W(TAG, "Unhandled card cryptogram size %d", card_cryptogram_len);
            return;
        }
        if(!seos_reader_verify_cryptogram(&seos_characteristic->params, card_cryptogram)) {
            FURI_LOG_W(TAG, "Card cryptogram failed verification");
            return;
        }
        FURI_LOG_I(TAG, "Authenticated");
        view_dispatcher_send_custom_event(
            seos_characteristic->seos->view_dispatcher, SeosCustomEventAuthenticated);

        if(seos_characteristic->secure_messaging) {
            secure_messaging_free(seos_characteristic->secure_messaging);
        }
        seos_characteristic->secure_messaging =
            secure_messaging_alloc(&seos_characteristic->params);
        if(!seos_characteristic->secure_messaging) {
            FURI_LOG_W(TAG, "Could not start secure messaging");
            return;
        }

        SecureMessaging* secure_messaging = seos_characteristic->secure_messaging;

        uint8_t message[] = {0x5c, 0x02, 0xff, 0x00};
        secure_messaging_wrap_apdu(
            secure_messaging,
            message,
            sizeof(message),
            (uint8_t*)SEOS_SM_HEADER,
            sizeof(SEOS_SM_HEADER),
            true,
            payload);
        seos_sio_collect_begin(
            &seos_characteristic->collector,
            seos_characteristic->assembled,
            bit_buffer_get_data(payload),
            bit_buffer_get_size_bytes(payload));
        seos_characteristic->phase = REQUEST_SIO;
        view_dispatcher_send_custom_event(
            seos_characteristic->seos->view_dispatcher, SeosCustomEventSIORequested);
    } else if(seos_characteristic->phase == REQUEST_SIO) {
        SecureMessaging* secure_messaging = seos_characteristic->secure_messaging;

        /* The answer may arrive in pieces, each asking to be continued. They
         * are one protected message, so nothing is unwrapped until the last. */
        SeosSioCollectResult collected =
            seos_sio_collect_step(&seos_characteristic->collector, rx_data, rx_len, payload);
        if(collected == SeosSioCollectSend) return;
        if(collected == SeosSioCollectFailed) {
            FURI_LOG_W(TAG, "Could not collect the read answer");
            return;
        }

        BitBuffer* rx_buffer = seos_characteristic->assembled;
        seos_log_bitbuffer(TAG, "BLE response(wrapped)", rx_buffer);
        if(!secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer)) {
            FURI_LOG_W(TAG, "Could not unwrap SIO response");
            return;
        }
        seos_log_bitbuffer(TAG, "BLE response(clear)", rx_buffer);

        SeosCredential* credential = seos_characteristic->credential;
        AuthParameters* params = &seos_characteristic->params;

        size_t sio_len = 0;
        if(!seos_parse_sio_response(
               bit_buffer_get_data(rx_buffer),
               bit_buffer_get_size_bytes(rx_buffer),
               credential->sio,
               sizeof(credential->sio),
               &sio_len)) {
            FURI_LOG_W(TAG, "No credential in the read answer");
            return;
        }
        credential->sio_len = sio_len;

        /* The keys the session was built from, and the OID it was selected
         * by, are what let the credential be emulated later. The NFC reader
         * and the native BLE reader both keep them; this one did not, so a
         * credential read over the dongle saved with none of it. */
        memcpy(credential->priv_key, params->priv_key, sizeof(credential->priv_key));
        memcpy(credential->auth_key, params->auth_key, sizeof(credential->auth_key));
        credential->adf_oid_len = SEOS_ADF_OID_LEN;
        memcpy(credential->adf_oid, SEOS_ADF_OID, sizeof(credential->adf_oid));

        FURI_LOG_I(TAG, "SIO Captured, %d bytes", credential->sio_len);

        Seos* seos = seos_characteristic->seos;
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerSuccess);

        seos_characteristic->phase = SELECT_AID;
    } else if(rx_data[0] == 0xe1) {
        //ignore
    } else {
        FURI_LOG_W(TAG, "No match for write request");
    }
}

void seos_characteristic_cred_flow(
    SeosCharacteristic* seos_characteristic,
    BitBuffer* attribute_value,
    BitBuffer* payload) {
    const uint8_t* apdu = NULL;
    size_t apdu_len = 0;
    if(!ble_apdu_bounds(attribute_value, &apdu, &apdu_len)) {
        FURI_LOG_I(TAG, "Message carries no command");
        return;
    }

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    const uint8_t* oid_list = NULL;
    size_t oid_list_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        if((aid_len == sizeof(standard_seos_aid) &&
            memcmp(aid, standard_seos_aid, aid_len) == 0)) {
            seos_emulator_select_aid(payload, aid, aid_len);
            bit_buffer_append_bytes(payload, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
        } else {
            bit_buffer_append_bytes(
                payload, (uint8_t*)SEOS_SW_FILE_NOT_FOUND, sizeof(SEOS_SW_FILE_NOT_FOUND));
        }
    } else if(seos_parse_select_adf(apdu, apdu_len, &oid_list, &oid_list_len)) {
        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_characteristic->params,
               seos_characteristic->credential,
               payload)) {
            bit_buffer_append_bytes(payload, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
        }

    } else if(seos_is_general_authenticate_1(apdu, apdu_len)) {
        seos_emulator_general_authenticate_1(payload, seos_characteristic->params);
        bit_buffer_append_bytes(payload, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
    } else if(seos_is_general_authenticate_2(apdu, apdu_len)) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               apdu_len,
               seos_characteristic->credential,
               &seos_characteristic->params,
               payload)) {
            /* The cryptogram did not verify, so the nonces it was built from
             * are not agreed. Answering well formed nonsense gives away no
             * more than a refusal would, and no session is started: keys
             * derived from unverified nonces are not a session. */
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
            bit_buffer_reset(payload);
            seos_emulator_shill_authenticate(payload);
        } else {
            bit_buffer_append_bytes(payload, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));

            view_dispatcher_send_custom_event(
                seos_characteristic->seos->view_dispatcher, SeosCustomEventAuthenticated);

            /* Replacing a session without freeing it leaks the old one. */
            if(seos_characteristic->secure_messaging) {
                secure_messaging_free(seos_characteristic->secure_messaging);
            }
            seos_characteristic->secure_messaging =
                secure_messaging_alloc(&seos_characteristic->params);
        }
    } else if(
        apdu_len >= SEOS_GET_RESPONSE_LEN &&
        memcmp(apdu, SEOS_GET_RESPONSE, SEOS_GET_RESPONSE_LEN - 1) == 0) {
        if(seos_characteristic->secure_messaging) {
            seos_sm_command_get_response(
                seos_characteristic->secure_messaging,
                SEOS_SM_MAX_FRAME,
                apdu[SEOS_GET_RESPONSE_LEN - 1],
                payload);
        } else {
            seos_sm_append_status(payload, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else if(seos_sm_command_matches(apdu, apdu_len)) {
        if(seos_characteristic->secure_messaging) {
            /* apdu already skips the leading BLE start byte. */
            if(!seos_sm_command_handle(
                   seos_characteristic->secure_messaging,
                   seos_characteristic->credential,
                   apdu,
                   apdu_len,
                   SEOS_SM_MAX_FRAME,
                   payload,
                   seos_sm_event_to_view_dispatcher,
                   seos_characteristic->seos)) {
                secure_messaging_free(seos_characteristic->secure_messaging);
                seos_characteristic->secure_messaging = NULL;
            }
        } else {
            seos_sm_append_status(payload, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else if(apdu[0] == 0xe1) {
        // ignore
    } else {
        FURI_LOG_W(TAG, "no match for attribute_value");
    }
}

/* Where a chunk goes: one ATT notification on a given handle. */
typedef struct {
    SeosAtt* seos_att;
    uint16_t handle;
} AttNotifyTarget;

static bool att_notify_chunk(void* context, const uint8_t* chunk, size_t chunk_len) {
    AttNotifyTarget* target = context;
    BitBuffer* tx = bit_buffer_alloc(chunk_len);
    bit_buffer_append_bytes(tx, chunk, chunk_len);
    seos_att_notify(target->seos_att, target->handle, tx);
    bit_buffer_free(tx);
    return true;
}

void seos_characteristic_att_notify_chunk(SeosAtt* seos_att, uint16_t handle, BitBuffer* payload) {
    AttNotifyTarget target = {.seos_att = seos_att, .handle = handle};
    seos_ble_chunk(
        bit_buffer_get_data(payload),
        bit_buffer_get_size_bytes(payload),
        att_notify_chunk,
        &target);
}

void seos_characteristic_write_request(void* context, BitBuffer* attribute_value) {
    SeosCharacteristic* seos_characteristic = (SeosCharacteristic*)context;
    seos_log_bitbuffer(TAG, "write request", attribute_value);

    BitBuffer* payload = bit_buffer_alloc(128); // TODO: MTU
    const uint8_t* data = bit_buffer_get_data(attribute_value);
    const size_t len = bit_buffer_get_size_bytes(attribute_value);

    SeosBleFrameResult frame = seos_ble_reassemble(seos_characteristic->rx_buffer, data, len);
    if(frame != SeosBleFrameComplete) {
        bit_buffer_free(payload);
        return;
    }

    if(seos_characteristic->flow_mode == FLOW_READER) {
        seos_characteristic_reader_flow(
            seos_characteristic, seos_characteristic->rx_buffer, payload);
    } else if(seos_characteristic->flow_mode == FLOW_CRED) {
        seos_characteristic_cred_flow(
            seos_characteristic, seos_characteristic->rx_buffer, payload);
    }

    if(bit_buffer_get_size_bytes(payload) > 0) {
        seos_characteristic_att_notify_chunk(
            seos_characteristic->seos_att, seos_characteristic->handle, payload);
    }

    bit_buffer_free(payload);
}

void seos_characteristic_on_subscribe(void* context, uint16_t handle) {
    SeosCharacteristic* seos_characteristic = (SeosCharacteristic*)context;
    FURI_LOG_D(TAG, "seos_characteristic_on_subscribe %04x", handle);
    /*
    if(seos_characteristic->handle != 0) {
        FURI_LOG_W(TAG, "Ignoring subscribe; already subscribed");
        return;
    }
    */

    seos_characteristic->handle = handle;

    // Send initial select
    uint8_t select_header[] = {0x00, 0xa4, 0x04, 0x00, (uint8_t)sizeof(standard_seos_aid)};

    BitBuffer* tx = bit_buffer_alloc(sizeof(select_header) + sizeof(standard_seos_aid));

    bit_buffer_append_bytes(tx, select_header, sizeof(select_header));
    bit_buffer_append_bytes(tx, standard_seos_aid, sizeof(standard_seos_aid));
    seos_log_bitbuffer(TAG, "initial select", tx);

    seos_characteristic_att_notify_chunk(
        seos_characteristic->seos_att, seos_characteristic->handle, tx);
    seos_characteristic->phase = SELECT_AID;
    bit_buffer_free(tx);
}
