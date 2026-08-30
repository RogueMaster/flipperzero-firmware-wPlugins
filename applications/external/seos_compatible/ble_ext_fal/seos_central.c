#include "seos_central_i.h"

#include "seos_sm_command.h"
#include "../seos_sm_event_ui.h"
#include "../ble_shared/seos_ble_framing.h"
#include "seos_common.h"

#define TAG "SeosCentral"

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};

SeosCentral* seos_central_alloc(Seos* seos) {
    SeosCentral* seos_central = malloc(sizeof(SeosCentral));
    memset(seos_central, 0, sizeof(SeosCentral));
    seos_central->seos = seos;
    seos_central->credential = seos->credential;

    seos_central->phase = SELECT_AID;
    // Using DES for greater compatibilty
    seos_central->params.cipher = TWO_KEY_3DES_CBC_MODE;
    seos_central->params.hash = SHA1;

    seos_worker_random_nonce(seos_central->params.rndICC, sizeof(seos_central->params.rndICC));
    seos_worker_random_nonce(seos_central->params.rNonce, sizeof(seos_central->params.rNonce));

    seos_central->secure_messaging = NULL;

    seos_central->seos_att = seos_att_alloc(seos);
    seos_att_set_notify_callback(seos_central->seos_att, seos_central_notify, seos_central);

    seos_central->rx_buffer = bit_buffer_alloc(128); // TODO: MTU

    return seos_central;
}

void seos_central_free(SeosCentral* seos_central) {
    furi_assert(seos_central);
    seos_att_free(seos_central->seos_att);
    bit_buffer_free(seos_central->rx_buffer);

    if(seos_central->secure_messaging) {
        secure_messaging_free(seos_central->secure_messaging);
    }

    free(seos_central);
}

void seos_central_start(SeosCentral* seos_central, FlowMode mode) {
    seos_att_start(seos_central->seos_att, BLE_CENTRAL, mode);
}

void seos_central_stop(SeosCentral* seos_central) {
    seos_att_stop(seos_central->seos_att);
}

/* One chunk onto the dongle's ATT link. */
static bool seos_central_send_chunk(void* context, const uint8_t* chunk, size_t chunk_len) {
    SeosCentral* seos_central = context;
    BitBuffer* tx = bit_buffer_alloc(chunk_len);
    bit_buffer_append_bytes(tx, chunk, chunk_len);
    seos_att_write_request(seos_central->seos_att, tx);
    bit_buffer_free(tx);
    return true;
}

void seos_central_notify(void* context, const uint8_t* buffer, size_t buffer_len) {
    SeosCentral* seos_central = (SeosCentral*)context;
    seos_log_buffer(TAG, "notify", (uint8_t*)buffer, buffer_len);

    SeosBleFrameResult frame = seos_ble_reassemble(seos_central->rx_buffer, buffer, buffer_len);
    if(frame != SeosBleFrameComplete) {
        return;
    }

    BitBuffer* response = bit_buffer_alloc(128);

    // Match name to nfc version for easier copying
    const uint8_t* apdu = bit_buffer_get_data(seos_central->rx_buffer);
    const size_t apdu_len = bit_buffer_get_size_bytes(seos_central->rx_buffer);

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    const uint8_t* oid_list = NULL;
    size_t oid_list_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        if((aid_len == sizeof(standard_seos_aid) &&
            memcmp(aid, standard_seos_aid, aid_len) == 0)) {
            seos_emulator_select_aid(response, aid, aid_len);
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
            seos_central->phase = SELECT_ADF;
        } else {
            bit_buffer_append_bytes(
                response, (uint8_t*)SEOS_SW_FILE_NOT_FOUND, sizeof(SEOS_SW_FILE_NOT_FOUND));
        }
    } else if(seos_parse_select_adf(apdu, apdu_len, &oid_list, &oid_list_len)) {
        if(seos_emulator_select_adf(
               oid_list, oid_list_len, &seos_central->params, seos_central->credential, response)) {
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
            seos_central->phase = GENERAL_AUTHENTICATION_1;
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
        }
    } else if(seos_is_general_authenticate_1(apdu, apdu_len)) {
        seos_emulator_general_authenticate_1(response, seos_central->params);

        bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
        seos_central->phase = GENERAL_AUTHENTICATION_2;
    } else if(seos_is_general_authenticate_2(apdu, apdu_len)) {
        if(seos_emulator_general_authenticate_2(
               apdu, apdu_len, seos_central->credential, &seos_central->params, response)) {
            FURI_LOG_I(TAG, "Authenticated");

            view_dispatcher_send_custom_event(
                seos_central->seos->view_dispatcher, SeosCustomEventAuthenticated);
            if(seos_central->secure_messaging) {
                secure_messaging_free(seos_central->secure_messaging);
            }
            seos_central->secure_messaging = secure_messaging_alloc(&seos_central->params);
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
            seos_central->phase = REQUEST_SIO;
        } else {
            /* Silence tells the other end its key was wrong just as clearly as
             * an error would. Well formed nonsense does not, and the phase
             * stays where it is: there is no session to carry it forward. */
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
            bit_buffer_reset(response);
            seos_emulator_shill_authenticate(response);
        }
    } else if(
        apdu_len >= SEOS_GET_RESPONSE_LEN &&
        memcmp(apdu, SEOS_GET_RESPONSE, SEOS_GET_RESPONSE_LEN - 1) == 0) {
        if(seos_central->secure_messaging) {
            seos_sm_command_get_response(
                seos_central->secure_messaging,
                SEOS_SM_MAX_FRAME,
                apdu[SEOS_GET_RESPONSE_LEN - 1],
                response);
        } else {
            seos_sm_append_status(response, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else if(seos_sm_command_matches(apdu, apdu_len)) {
        if(seos_central->secure_messaging) {
            if(!seos_sm_command_handle(
                   seos_central->secure_messaging,
                   seos_central->credential,
                   apdu,
                   apdu_len,
                   SEOS_SM_MAX_FRAME,
                   response,
                   seos_sm_event_to_view_dispatcher,
                   seos_central->seos)) {
                secure_messaging_free(seos_central->secure_messaging);
                seos_central->secure_messaging = NULL;
            }
        } else {
            seos_sm_append_status(response, SECURE_MESSAGING_SW_INCORRECT_DO);
        }

    } else {
        FURI_LOG_W(TAG, "no match for attribute_value");
    }

    if(bit_buffer_get_size_bytes(response) > 0) {
        seos_ble_chunk(
            bit_buffer_get_data(response),
            bit_buffer_get_size_bytes(response),
            seos_central_send_chunk,
            seos_central);
    }
    bit_buffer_free(response);
}
