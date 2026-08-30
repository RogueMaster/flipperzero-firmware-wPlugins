#include "seos_native_peripheral_i.h"

#include "seos_sm_command.h"
#include "../seos_sm_event_ui.h"
#include "../ble_shared/seos_ble_framing.h"

#define TAG "SeosNativePeripheral"

#define MESSAGE_QUEUE_SIZE 10

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t cd02[] = {0xcd, 0x02};
static uint8_t ga1_response[] = {0x7c, 0x0a, 0x81, 0x08};

// Emulation

int32_t seos_native_peripheral_task(void* context);

typedef struct {
    size_t len;
    uint8_t buf[BLE_SVC_SEOS_CHAR_VALUE_LEN_MAX];
} NativePeripheralMessage;

static void seos_ble_connection_status_callback(BtStatus status, void* context) {
    furi_assert(context);
    SeosNativePeripheral* seos_native_peripheral = context;
    if(status == BtStatusConnected) {
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventConnected);
    } else if(status == BtStatusAdvertising) {
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAdvertising);
    }
}

static uint16_t seos_svc_callback(SeosServiceEvent event, void* context) {
    SeosNativePeripheral* seos_native_peripheral = context;
    uint16_t bytes_available = 0;

    if(event.event == SeosServiceEventTypeDataReceived) {
        uint32_t space = furi_message_queue_get_space(seos_native_peripheral->messages);
        if(space > 0) {
            if(event.data.size > sizeof(((NativePeripheralMessage*)0)->buf)) {
                /* The peer states this length. A write longer than the
                 * characteristic holds is dropped rather than copied. */
                FURI_LOG_W(TAG, "Write of %d bytes will not fit", event.data.size);
                return 0;
            }
            NativePeripheralMessage message = {.len = event.data.size};
            memcpy(message.buf, event.data.buffer, event.data.size);

            if(furi_mutex_acquire(seos_native_peripheral->mq_mutex, FuriWaitForever) ==
               FuriStatusOk) {
                furi_message_queue_put(
                    seos_native_peripheral->messages, &message, FuriWaitForever);
                furi_mutex_release(seos_native_peripheral->mq_mutex);
            }
            if(space < MESSAGE_QUEUE_SIZE / 2) {
                // Log if queue is more than half full, but still accept messages to avoid clogging the queue
                FURI_LOG_D(TAG, "Queue message.  %ld remaining", space);
            }
            bytes_available = (space - 1) * sizeof(NativePeripheralMessage);
        } else {
            FURI_LOG_E(TAG, "No space in message queue");
        }
    }

    return bytes_available;
}

SeosNativePeripheral* seos_native_peripheral_alloc(Seos* seos) {
    SeosNativePeripheral* seos_native_peripheral = malloc(sizeof(SeosNativePeripheral));
    memset(seos_native_peripheral, 0, sizeof(SeosNativePeripheral));

    seos_native_peripheral->seos = seos;
    seos_native_peripheral->credential = seos->credential;
    seos_native_peripheral->bt = furi_record_open(RECORD_BT);

    seos_native_peripheral->phase = SELECT_AID;
    seos_native_peripheral->secure_messaging = NULL;
    seos_native_peripheral->params.key_no = 1;
    seos_worker_random_nonce(
        seos_native_peripheral->params.cNonce, sizeof(seos_native_peripheral->params.cNonce));
    seos_worker_random_nonce(
        seos_native_peripheral->params.UID, sizeof(seos_native_peripheral->params.UID));

    seos_native_peripheral->thread = furi_thread_alloc_ex(
        "SeosNativePeripheralWorker",
        5 * 1024,
        seos_native_peripheral_task,
        seos_native_peripheral);
    seos_native_peripheral->messages =
        furi_message_queue_alloc(MESSAGE_QUEUE_SIZE, sizeof(NativePeripheralMessage));
    seos_native_peripheral->mq_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    seos_native_peripheral->assembled = bit_buffer_alloc(SEOS_SM_RESPONSE_MAX);
    seos_native_peripheral->rx_buffer = bit_buffer_alloc(128); // TODO: MTU

    return seos_native_peripheral;
}

void seos_native_peripheral_free(SeosNativePeripheral* seos_native_peripheral) {
    furi_assert(seos_native_peripheral);

    furi_record_close(RECORD_BT);

    furi_message_queue_free(seos_native_peripheral->messages);
    furi_mutex_free(seos_native_peripheral->mq_mutex);
    furi_thread_free(seos_native_peripheral->thread);

    bit_buffer_free(seos_native_peripheral->rx_buffer);
    bit_buffer_free(seos_native_peripheral->assembled);

    if(seos_native_peripheral->secure_messaging) {
        secure_messaging_free(seos_native_peripheral->secure_messaging);
    }

    free(seos_native_peripheral);
}

void seos_native_peripheral_start(SeosNativePeripheral* seos_native_peripheral, FlowMode mode) {
    seos_native_peripheral->flow_mode = mode;
    if(seos_native_peripheral->flow_mode == FLOW_CRED) {
        seos_native_peripheral->params.key_no = 0;
        seos_native_peripheral->params.cipher = TWO_KEY_3DES_CBC_MODE;
        seos_native_peripheral->params.hash = SHA1;

        seos_worker_random_nonce(
            seos_native_peripheral->params.rndICC, sizeof(seos_native_peripheral->params.rndICC));
        seos_worker_random_nonce(
            seos_native_peripheral->params.rNonce, sizeof(seos_native_peripheral->params.rNonce));
        memset(
            seos_native_peripheral->params.UID, 0x00, sizeof(seos_native_peripheral->params.UID));
        memset(
            seos_native_peripheral->params.cNonce,
            0x00,
            sizeof(seos_native_peripheral->params.cNonce));
    }

    bt_disconnect(seos_native_peripheral->bt);

    BleProfileParams params = {
        .mode = mode,
    };

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);
    seos_native_peripheral->ble_profile =
        bt_profile_start(seos_native_peripheral->bt, ble_profile_seos, &params);
    furi_check(seos_native_peripheral->ble_profile);
    bt_set_status_changed_callback(
        seos_native_peripheral->bt, seos_ble_connection_status_callback, seos_native_peripheral);
    ble_profile_seos_set_event_callback(
        seos_native_peripheral->ble_profile,
        sizeof(seos_native_peripheral->event_buffer),
        seos_svc_callback,
        seos_native_peripheral);
    furi_hal_bt_start_advertising();
    view_dispatcher_send_custom_event(
        seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAdvertising);

    furi_thread_start(seos_native_peripheral->thread);
}

void seos_native_peripheral_stop(SeosNativePeripheral* seos_native_peripheral) {
    furi_hal_bt_stop_advertising();
    bt_set_status_changed_callback(seos_native_peripheral->bt, NULL, NULL);
    bt_disconnect(seos_native_peripheral->bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(seos_native_peripheral->bt);

    furi_check(bt_profile_restore_default(seos_native_peripheral->bt));

    furi_thread_flags_set(
        furi_thread_get_id(seos_native_peripheral->thread), NativePeripheralEvtStop);
    furi_thread_join(seos_native_peripheral->thread);
}

void seos_native_peripheral_process_message_cred(
    SeosNativePeripheral* seos_native_peripheral,
    NativePeripheralMessage message) {
    Seos* seos = seos_native_peripheral->seos;
    BitBuffer* response = bit_buffer_alloc(128); // TODO: MTU

    SeosBleFrameResult frame =
        seos_ble_reassemble(seos_native_peripheral->rx_buffer, message.buf, message.len);
    if(frame != SeosBleFrameComplete) {
        bit_buffer_free(response);
        return;
    }

    const uint8_t* apdu = bit_buffer_get_data(seos_native_peripheral->rx_buffer);
    const size_t apdu_len = bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer);

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    const uint8_t* oid_list = NULL;
    size_t oid_list_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        if((aid_len == sizeof(standard_seos_aid) &&
            memcmp(aid, standard_seos_aid, aid_len) == 0)) {
            seos_emulator_select_aid(response, aid, aid_len);
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
        } else {
            bit_buffer_append_bytes(
                response, (uint8_t*)SEOS_SW_FILE_NOT_FOUND, sizeof(SEOS_SW_FILE_NOT_FOUND));
        }
    } else if(seos_parse_select_adf(apdu, apdu_len, &oid_list, &oid_list_len)) {
        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_native_peripheral->params,
               seos_native_peripheral->credential,
               response)) {
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventADFMatched);
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
        }

    } else if(seos_is_general_authenticate_1(apdu, apdu_len)) {
        seos_emulator_general_authenticate_1(response, seos_native_peripheral->params);
        bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
    } else if(seos_is_general_authenticate_2(apdu, apdu_len)) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               apdu_len,
               seos_native_peripheral->credential,
               &seos_native_peripheral->params,
               response)) {
            /* The cryptogram did not verify, so the nonces it was built from
             * are not agreed. Answering well formed nonsense gives away no
             * more than a refusal would, and no session is started: keys
             * derived from unverified nonces are not a session. */
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
            bit_buffer_reset(response);
            seos_emulator_shill_authenticate(response);
        } else {
            bit_buffer_append_bytes(response, (uint8_t*)SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));

            view_dispatcher_send_custom_event(
                seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAuthenticated);

            /* Replacing a session without freeing it leaks the old one. */
            if(seos_native_peripheral->secure_messaging) {
                secure_messaging_free(seos_native_peripheral->secure_messaging);
            }
            seos_native_peripheral->secure_messaging =
                secure_messaging_alloc(&seos_native_peripheral->params);
        }
    } else if(
        apdu_len >= SEOS_GET_RESPONSE_LEN &&
        memcmp(apdu, SEOS_GET_RESPONSE, SEOS_GET_RESPONSE_LEN - 1) == 0) {
        if(seos_native_peripheral->secure_messaging) {
            seos_sm_command_get_response(
                seos_native_peripheral->secure_messaging,
                SEOS_SM_MAX_FRAME,
                apdu[SEOS_GET_RESPONSE_LEN - 1],
                response);
        } else {
            seos_sm_append_status(response, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else if(seos_sm_command_matches(apdu, apdu_len)) {
        if(seos_native_peripheral->secure_messaging) {
            if(!seos_sm_command_handle(
                   seos_native_peripheral->secure_messaging,
                   seos_native_peripheral->credential,
                   apdu,
                   apdu_len,
                   SEOS_SM_MAX_FRAME,
                   response,
                   seos_sm_event_to_view_dispatcher,
                   seos_native_peripheral->seos)) {
                secure_messaging_free(seos_native_peripheral->secure_messaging);
                seos_native_peripheral->secure_messaging = NULL;
            }
        } else {
            seos_sm_append_status(response, SECURE_MESSAGING_SW_INCORRECT_DO);
        }
    } else {
        FURI_LOG_W(TAG, "no match for message");
    }

    if(bit_buffer_get_size_bytes(response) > 0) {
        ble_profile_seos_tx(
            seos_native_peripheral->ble_profile,
            (uint8_t*)bit_buffer_get_data(response),
            bit_buffer_get_size_bytes(response));
    }

    bit_buffer_free(response);
}

void seos_native_peripheral_process_message_reader(
    SeosNativePeripheral* seos_native_peripheral,
    NativePeripheralMessage message) {
    SeosBleFrameResult frame =
        seos_ble_reassemble(seos_native_peripheral->rx_buffer, message.buf, message.len);
    if(frame != SeosBleFrameComplete) {
        return;
    }

    BitBuffer* response = bit_buffer_alloc(128); // TODO: MTU

    const uint8_t* rx_data = bit_buffer_get_data(seos_native_peripheral->rx_buffer);
    const size_t rx_len = bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer);

    /* Every test below reads into the response, so there has to be one. */
    if(rx_len == 0) {
        FURI_LOG_I(TAG, "Empty response");
        bit_buffer_free(response);
        return;
    }

    /* The select answer names the application four bytes in. */
    const size_t select_aid_offset = 4;

    const uint8_t* card_cryptogram = NULL;
    size_t card_cryptogram_len = 0;

    if(rx_len >= select_aid_offset + sizeof(standard_seos_aid) &&
       memcmp(rx_data + select_aid_offset, standard_seos_aid, sizeof(standard_seos_aid)) ==
           0) { // response to select
        FURI_LOG_I(TAG, "Select ADF");
        uint8_t select_adf_header[] = {
            0x80, 0xa5, 0x04, 0x00, (uint8_t)SEOS_ADF_OID_LEN + 2, 0x06, (uint8_t)SEOS_ADF_OID_LEN};

        bit_buffer_append_bytes(response, select_adf_header, sizeof(select_adf_header));
        bit_buffer_append_bytes(response, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
        bit_buffer_append_byte(response, 0x00);
        seos_native_peripheral->phase = SELECT_ADF;
    } else if(
        seos_native_peripheral->phase == SELECT_ADF && rx_len >= sizeof(SEOS_SW_FILE_NOT_FOUND) &&
        memcmp(rx_data, SEOS_SW_FILE_NOT_FOUND, sizeof(SEOS_SW_FILE_NOT_FOUND)) == 0) {
        // Our ADF OID was rejected, close the connection
        FURI_LOG_W(TAG, "Failed to match ADF OID");
        bt_disconnect(seos_native_peripheral->bt);

        // Revert UI to advertising state
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAdvertising);
    } else if(rx_len >= sizeof(cd02) && memcmp(rx_data, cd02, sizeof(cd02)) == 0) {
        BitBuffer* attribute_value = bit_buffer_alloc(rx_len);
        bit_buffer_append_bytes(attribute_value, rx_data, rx_len);
        if(seos_reader_select_adf_response(
               attribute_value,
               0,
               seos_native_peripheral->credential,
               &seos_native_peripheral->params)) {
            // Craft response
            uint8_t general_authenticate_1[SEOS_GENERAL_AUTHENTICATE_1_LEN];
            seos_build_general_authenticate_1(
                seos_native_peripheral->params.key_no, general_authenticate_1);
            bit_buffer_append_bytes(
                response, general_authenticate_1, sizeof(general_authenticate_1));
            seos_native_peripheral->phase = GENERAL_AUTHENTICATION_1;
        }
        bit_buffer_free(attribute_value);
    } else if(
        rx_len >= sizeof(ga1_response) &&
        memcmp(rx_data, ga1_response, sizeof(ga1_response)) == 0 &&
        seos_parse_ga1_response(
            rx_data,
            rx_len,
            seos_native_peripheral->params.rndICC,
            sizeof(seos_native_peripheral->params.rndICC))) {
        // Craft response
        uint8_t cryptogram[32 + 8];
        memset(cryptogram, 0, sizeof(cryptogram));
        seos_reader_generate_cryptogram(
            seos_native_peripheral->credential, &seos_native_peripheral->params, cryptogram);

        uint8_t ga_header[] = {
            0x00,
            0x87,
            0x00,
            seos_native_peripheral->params.key_no,
            sizeof(cryptogram) + 4,
            0x7c,
            sizeof(cryptogram) + 2,
            0x82,
            sizeof(cryptogram)};

        bit_buffer_append_bytes(response, ga_header, sizeof(ga_header));
        bit_buffer_append_bytes(response, cryptogram, sizeof(cryptogram));
        bit_buffer_append_byte(response, 0x00);

        seos_native_peripheral->phase = GENERAL_AUTHENTICATION_2;
    } else if(seos_parse_ga2_response(rx_data, rx_len, &card_cryptogram, &card_cryptogram_len)) {
        /* Nothing past here happens unless the card proved it holds the key:
         * a session built on an unverified cryptogram is not a session. */
        if(card_cryptogram_len != SEOS_CARD_CRYPTOGRAM_LEN) {
            FURI_LOG_W(TAG, "Unhandled card cryptogram size %d", card_cryptogram_len);
            bit_buffer_free(response);
            return;
        }
        if(!seos_reader_verify_cryptogram(&seos_native_peripheral->params, card_cryptogram)) {
            FURI_LOG_W(TAG, "Card cryptogram failed verification");
            bit_buffer_free(response);
            return;
        }
        FURI_LOG_I(TAG, "Authenticated");
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAuthenticated);

        if(seos_native_peripheral->secure_messaging) {
            secure_messaging_free(seos_native_peripheral->secure_messaging);
        }
        seos_native_peripheral->secure_messaging =
            secure_messaging_alloc(&seos_native_peripheral->params);
        if(!seos_native_peripheral->secure_messaging) {
            FURI_LOG_W(TAG, "Could not start secure messaging");
            bit_buffer_free(response);
            return;
        }

        SecureMessaging* secure_messaging = seos_native_peripheral->secure_messaging;

        uint8_t message[] = {0x5c, 0x02, 0xff, 0x00};
        secure_messaging_wrap_apdu(
            secure_messaging,
            message,
            sizeof(message),
            (uint8_t*)SEOS_SM_HEADER,
            sizeof(SEOS_SM_HEADER),
            true,
            response);
        seos_sio_collect_begin(
            &seos_native_peripheral->collector,
            seos_native_peripheral->assembled,
            bit_buffer_get_data(response),
            bit_buffer_get_size_bytes(response));
        seos_native_peripheral->phase = REQUEST_SIO;
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventSIORequested);
    } else if(seos_native_peripheral->phase == REQUEST_SIO) {
        SecureMessaging* secure_messaging = seos_native_peripheral->secure_messaging;
        SeosCredential* credential = seos_native_peripheral->credential;
        AuthParameters* params = &seos_native_peripheral->params;

        /* The answer may arrive in pieces, each asking to be continued. They
         * are one protected message, so nothing is unwrapped until the last.
         * The collector takes the status word off each piece; this used to
         * drop a single trailing byte, which is not the status word. */
        SeosSioCollectResult collected =
            seos_sio_collect_step(&seos_native_peripheral->collector, rx_data, rx_len, response);
        if(collected != SeosSioCollectComplete) {
            /* Still collecting: whatever the step put in `response` is sent by
             * the tail of this function, as any other answer would be. */
            if(collected == SeosSioCollectFailed) {
                FURI_LOG_W(TAG, "Could not collect the read answer");
                bit_buffer_reset(response);
            }
        } else {
            BitBuffer* rx_buffer = seos_native_peripheral->assembled;
            seos_log_bitbuffer(TAG, "BLE response(wrapped)", rx_buffer);
            if(!secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer)) {
                FURI_LOG_W(TAG, "Could not unwrap SIO response");
                bit_buffer_free(response);
                return;
            }
            seos_log_bitbuffer(TAG, "BLE response(clear)", rx_buffer);

            size_t sio_len = 0;
            if(!seos_parse_sio_response(
                   bit_buffer_get_data(rx_buffer),
                   bit_buffer_get_size_bytes(rx_buffer),
                   credential->sio,
                   sizeof(credential->sio),
                   &sio_len)) {
                FURI_LOG_W(TAG, "No credential in the read answer");
                bit_buffer_free(response);
                return;
            }
            credential->sio_len = sio_len;
            memcpy(credential->priv_key, params->priv_key, sizeof(credential->priv_key));
            memcpy(credential->auth_key, params->auth_key, sizeof(credential->auth_key));
            credential->adf_oid_len = SEOS_ADF_OID_LEN;
            memcpy(credential->adf_oid, SEOS_ADF_OID, sizeof(credential->adf_oid));

            FURI_LOG_I(TAG, "SIO Captured, %d bytes", credential->sio_len);

            Seos* seos = seos_native_peripheral->seos;
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerSuccess);

            seos_native_peripheral->phase = SELECT_AID;
        }

    } else {
        FURI_LOG_W(TAG, "No match for write request");
        seos_log_buffer(TAG, "No match for reader incoming", (uint8_t*)rx_data, rx_len);
    }

    if(bit_buffer_get_size_bytes(response) > 0) {
        ble_profile_seos_tx(
            seos_native_peripheral->ble_profile,
            (uint8_t*)bit_buffer_get_data(response),
            bit_buffer_get_size_bytes(response));
    }

    bit_buffer_free(response);
}

int32_t seos_native_peripheral_task(void* context) {
    SeosNativePeripheral* seos_native_peripheral = (SeosNativePeripheral*)context;
    bool running = true;

    while(running) {
        uint32_t events = furi_thread_flags_get();
        if(events & NativePeripheralEvtStop) {
            running = false;
            break;
        }

        if(furi_mutex_acquire(seos_native_peripheral->mq_mutex, 1) == FuriStatusOk) {
            uint32_t count = furi_message_queue_get_count(seos_native_peripheral->messages);
            if(count > 0) {
                if(count > MESSAGE_QUEUE_SIZE / 2) {
                    // Log if queue is more than half full, but still process all messages to avoid clogging the queue
                    FURI_LOG_I(TAG, "Dequeue message [%ld messages]", count);
                }

                NativePeripheralMessage message = {};
                FuriStatus status = furi_message_queue_get(
                    seos_native_peripheral->messages, &message, FuriWaitForever);
                if(status != FuriStatusOk) {
                    FURI_LOG_W(TAG, "furi_message_queue_get fail %d", status);
                }

                if(seos_native_peripheral->flow_mode == FLOW_READER) {
                    seos_native_peripheral_process_message_reader(seos_native_peripheral, message);
                } else if(seos_native_peripheral->flow_mode == FLOW_CRED) {
                    seos_native_peripheral_process_message_cred(seos_native_peripheral, message);
                }
            }
            furi_mutex_release(seos_native_peripheral->mq_mutex);
        } else {
            FURI_LOG_W(TAG, "Failed to acquire mutex");
        }

        // A beat for event flags
        furi_delay_ms(1);
    }

    return 0;
}
