#include "seos_native_peripheral_i.h"

#define TAG "SeosNativePeripheral"

#define MESSAGE_QUEUE_SIZE 10

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t cd02[] = {0xcd, 0x02};
static uint8_t general_authenticate_1[] =
    {0x00, 0x87, 0x00, 0x01, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};
static uint8_t ga1_response[] = {0x7c, 0x0a, 0x81, 0x08};

// Emulation
static uint8_t success[] = {0x90, 0x00};
static uint8_t file_not_found[] = {0x6A, 0x82};

static uint8_t select_header[] = {0x00, 0xa4, 0x04, 0x00};
static uint8_t select_adf_header[] = {0x80, 0xa5, 0x04, 0x00};
static uint8_t general_authenticate_2_header[] = {0x00, 0x87, 0x00, 0x01};
static uint8_t secure_messaging_header[] = {0x0c, 0xcb, 0x3f, 0xff};

static uint8_t apdu_header[] = {0x0c, 0xcb, 0x3f, 0xff};

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
    memset(
        seos_native_peripheral->params.cNonce,
        0x0c,
        sizeof(seos_native_peripheral->params.cNonce));
    memset(seos_native_peripheral->params.UID, 0x0d, sizeof(seos_native_peripheral->params.UID));

    seos_native_peripheral->thread = furi_thread_alloc_ex(
        "SeosNativePeripheralWorker",
        5 * 1024,
        seos_native_peripheral_task,
        seos_native_peripheral);
    seos_native_peripheral->messages =
        furi_message_queue_alloc(MESSAGE_QUEUE_SIZE, sizeof(NativePeripheralMessage));
    seos_native_peripheral->mq_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

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

    free(seos_native_peripheral);
}

void seos_native_peripheral_start(SeosNativePeripheral* seos_native_peripheral, FlowMode mode) {
    seos_native_peripheral->flow_mode = mode;
    if(seos_native_peripheral->flow_mode == FLOW_CRED) {
        seos_native_peripheral->params.key_no = 0;
        seos_native_peripheral->params.cipher = TWO_KEY_3DES_CBC_MODE;
        seos_native_peripheral->params.hash = SHA1;

        memset(
            seos_native_peripheral->params.rndICC,
            0x0d,
            sizeof(seos_native_peripheral->params.rndICC));
        memset(
            seos_native_peripheral->params.rNonce,
            0x0c,
            sizeof(seos_native_peripheral->params.rNonce));
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

    furi_thread_flags_set(furi_thread_get_id(seos_native_peripheral->thread), WorkerEvtStop);
    furi_thread_join(seos_native_peripheral->thread);
}

void seos_native_peripheral_process_message_cred(
    SeosNativePeripheral* seos_native_peripheral,
    NativePeripheralMessage message) {
    Seos* seos = seos_native_peripheral->seos;
    BitBuffer* response = bit_buffer_alloc(128); // TODO: MTU

    uint8_t flags = message.buf[0];

    // Check for error flag
    if((flags & BLE_FLAG_ERR) == BLE_FLAG_ERR) {
        seos_log_buffer(TAG, "Received error response", message.buf + 1, message.len - 1);
        return;
    }

    // Check for start-of-message flag
    if((flags & BLE_FLAG_SOM) == BLE_FLAG_SOM) {
        bit_buffer_reset(seos_native_peripheral->rx_buffer);
    } else {
        if(bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer) == 0) {
            FURI_LOG_W(TAG, "Expected start of BLE packet");
            return;
        }
    }

    bit_buffer_append_bytes(seos_native_peripheral->rx_buffer, message.buf + 1, message.len - 1);

    // Only parse if end-of-message flag found
    if((flags & BLE_FLAG_EOM) != BLE_FLAG_EOM) return;

    const uint8_t* apdu = bit_buffer_get_data(seos_native_peripheral->rx_buffer);
    const size_t apdu_len = bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer);

    if(memcmp(apdu, select_header, sizeof(select_header)) == 0) {
        if(memcmp(apdu + sizeof(select_header) + 1, standard_seos_aid, sizeof(standard_seos_aid)) ==
           0) {
            seos_emulator_select_aid(
                response, apdu + sizeof(select_header) + 1, sizeof(standard_seos_aid));
            bit_buffer_append_bytes(response, (uint8_t*)success, sizeof(success));
        } else {
            bit_buffer_append_bytes(response, (uint8_t*)file_not_found, sizeof(file_not_found));
        }
    } else if(memcmp(apdu, select_adf_header, sizeof(select_adf_header)) == 0) {
        // +1 to skip APDU length byte
        const uint8_t* oid_list = apdu + sizeof(select_adf_header) + 1;
        size_t oid_list_len = apdu[sizeof(select_adf_header)];

        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_native_peripheral->params,
               seos_native_peripheral->credential,
               response)) {
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventADFMatched);
            bit_buffer_append_bytes(response, (uint8_t*)success, sizeof(success));
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
        }

    } else if(memcmp(apdu, general_authenticate_1, sizeof(general_authenticate_1)) == 0) {
        seos_emulator_general_authenticate_1(response, seos_native_peripheral->params);
        bit_buffer_append_bytes(response, (uint8_t*)success, sizeof(success));
    } else if(memcmp(apdu, general_authenticate_2_header, sizeof(general_authenticate_2_header)) == 0) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               apdu_len,
               seos_native_peripheral->credential,
               &seos_native_peripheral->params,
               response)) {
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
        } else {
            bit_buffer_append_bytes(response, (uint8_t*)success, sizeof(success));
        }

        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAuthenticated);
        // Prepare for future communication
        seos_native_peripheral->secure_messaging =
            secure_messaging_alloc(&seos_native_peripheral->params);
    } else if(memcmp(apdu, secure_messaging_header, sizeof(secure_messaging_header)) == 0) {
        uint8_t request_sio[] = {0x5c, 0x02, 0xff, 0x00};

        if(seos_native_peripheral->secure_messaging) {
            FURI_LOG_D(TAG, "Unwrap secure message");

            // 0ccb3fff 16 8508fa8395d30de4e8e097008e085da7edbd833b002d00
            BitBuffer* tmp = bit_buffer_alloc(apdu_len);
            bit_buffer_append_bytes(tmp, apdu, apdu_len);

            seos_log_bitbuffer(TAG, "received(wrapped)", tmp);
            secure_messaging_unwrap_apdu(seos_native_peripheral->secure_messaging, tmp);
            seos_log_bitbuffer(TAG, "received(clear)", tmp);

            const uint8_t* message = bit_buffer_get_data(tmp);
            if(memcmp(message, request_sio, sizeof(request_sio)) == 0) {
                view_dispatcher_send_custom_event(
                    seos_native_peripheral->seos->view_dispatcher, SeosCustomEventSIORequested);

                BitBuffer* sio_file = bit_buffer_alloc(128);
                bit_buffer_append_bytes(sio_file, message + 2, 2); // fileId
                bit_buffer_append_byte(sio_file, seos_native_peripheral->credential->sio_len);
                bit_buffer_append_bytes(
                    sio_file,
                    seos_native_peripheral->credential->sio,
                    seos_native_peripheral->credential->sio_len);

                secure_messaging_wrap_rapdu(
                    seos_native_peripheral->secure_messaging,
                    (uint8_t*)bit_buffer_get_data(sio_file),
                    bit_buffer_get_size_bytes(sio_file),
                    response);
                bit_buffer_append_bytes(response, (uint8_t*)success, sizeof(success));

                bit_buffer_free(sio_file);
            }
        } else {
            uint8_t no_sm[] = {0x69, 0x88};
            bit_buffer_append_bytes(response, no_sm, sizeof(no_sm));
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
    uint8_t flags = message.buf[0];

    // Check for error flag
    if((flags & BLE_FLAG_ERR) == BLE_FLAG_ERR) {
        seos_log_buffer(TAG, "Received error response", message.buf + 1, message.len - 1);
        return;
    }

    // Check for start-of-message flag
    if((flags & BLE_FLAG_SOM) == BLE_FLAG_SOM) {
        bit_buffer_reset(seos_native_peripheral->rx_buffer);
    } else {
        if(bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer) == 0) {
            FURI_LOG_W(TAG, "Expected start of BLE packet");
            return;
        }
    }

    bit_buffer_append_bytes(seos_native_peripheral->rx_buffer, message.buf + 1, message.len - 1);

    // Only parse if end-of-message flag found
    if((flags & BLE_FLAG_EOM) != BLE_FLAG_EOM) return;

    BitBuffer* response = bit_buffer_alloc(128); // TODO: MTU

    const uint8_t* rx_data = bit_buffer_get_data(seos_native_peripheral->rx_buffer);
    const size_t rx_len = bit_buffer_get_size_bytes(seos_native_peripheral->rx_buffer);

    if(memcmp(rx_data + 4, standard_seos_aid, sizeof(standard_seos_aid)) ==
       0) { // response to select
        FURI_LOG_I(TAG, "Select ADF");
        uint8_t select_adf_header[] = {
            0x80, 0xa5, 0x04, 0x00, (uint8_t)SEOS_ADF_OID_LEN + 2, 0x06, (uint8_t)SEOS_ADF_OID_LEN};

        bit_buffer_append_bytes(response, select_adf_header, sizeof(select_adf_header));
        bit_buffer_append_bytes(response, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
        bit_buffer_append_byte(response, 0x00);
        seos_native_peripheral->phase = SELECT_ADF;
    } else if(
        seos_native_peripheral->phase == SELECT_ADF &&
        memcmp(rx_data, file_not_found, sizeof(file_not_found)) == 0) {
        // Our ADF OID was rejected, close the connection
        FURI_LOG_W(TAG, "Failed to match ADF OID");
        bt_disconnect(seos_native_peripheral->bt);

        // Revert UI to advertising state
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAdvertising);
    } else if(memcmp(rx_data, cd02, sizeof(cd02)) == 0) {
        BitBuffer* attribute_value = bit_buffer_alloc(rx_len);
        bit_buffer_append_bytes(attribute_value, rx_data, rx_len);
        if(seos_reader_select_adf_response(
               attribute_value,
               0,
               seos_native_peripheral->credential,
               &seos_native_peripheral->params)) {
            // Craft response
            general_authenticate_1[3] = seos_native_peripheral->params.key_no;
            bit_buffer_append_bytes(
                response, general_authenticate_1, sizeof(general_authenticate_1));
            seos_native_peripheral->phase = GENERAL_AUTHENTICATION_1;
        }
        bit_buffer_free(attribute_value);
    } else if(memcmp(rx_data, ga1_response, sizeof(ga1_response)) == 0) {
        memcpy(seos_native_peripheral->params.rndICC, rx_data + 4, 8);

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
    } else if(rx_data[0] == 0x7C && rx_data[2] == 0x82) { // ga2 response
        if(rx_data[3] == 40) {
            if(!seos_reader_verify_cryptogram(&seos_native_peripheral->params, rx_data + 4)) {
                FURI_LOG_W(TAG, "Card cryptogram failed verification");
                bit_buffer_free(response);
                return;
            }
            FURI_LOG_I(TAG, "Authenticated");
            view_dispatcher_send_custom_event(
                seos_native_peripheral->seos->view_dispatcher, SeosCustomEventAuthenticated);
        } else {
            FURI_LOG_W(TAG, "Unhandled card cryptogram size %d", rx_data[3]);
        }

        seos_native_peripheral->secure_messaging =
            secure_messaging_alloc(&seos_native_peripheral->params);

        SecureMessaging* secure_messaging = seos_native_peripheral->secure_messaging;

        uint8_t message[] = {0x5c, 0x02, 0xff, 0x00};
        secure_messaging_wrap_apdu(
            secure_messaging, message, sizeof(message), apdu_header, sizeof(apdu_header), response);
        seos_native_peripheral->phase = REQUEST_SIO;
        view_dispatcher_send_custom_event(
            seos_native_peripheral->seos->view_dispatcher, SeosCustomEventSIORequested);
    } else if(seos_native_peripheral->phase == REQUEST_SIO) {
        // TODO: consider seos_reader_request_sio
        SecureMessaging* secure_messaging = seos_native_peripheral->secure_messaging;
        SeosCredential* credential = seos_native_peripheral->credential;
        AuthParameters* params = &seos_native_peripheral->params;

        BitBuffer* rx_buffer = bit_buffer_alloc(rx_len - 1);
        bit_buffer_append_bytes(rx_buffer, rx_data, rx_len - 1);
        seos_log_bitbuffer(TAG, "BLE response(wrapped)", rx_buffer);
        secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer);
        seos_log_bitbuffer(TAG, "BLE response(clear)", rx_buffer);

        // Skip fileId
        credential->sio_len = bit_buffer_get_byte(rx_buffer, 2);
        if(credential->sio_len > sizeof(credential->sio)) {
            FURI_LOG_W(TAG, "SIO too long to save");
            bit_buffer_free(response);
            return;
        }
        memcpy(credential->sio, bit_buffer_get_data(rx_buffer) + 3, credential->sio_len);
        memcpy(credential->priv_key, params->priv_key, sizeof(credential->priv_key));
        memcpy(credential->auth_key, params->auth_key, sizeof(credential->auth_key));
        credential->adf_oid_len = SEOS_ADF_OID_LEN;
        memcpy(credential->adf_oid, SEOS_ADF_OID, sizeof(credential->adf_oid));

        FURI_LOG_I(TAG, "SIO Captured, %d bytes", credential->sio_len);

        Seos* seos = seos_native_peripheral->seos;
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerSuccess);
        bit_buffer_free(rx_buffer);

        seos_native_peripheral->phase = SELECT_AID;

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
        if(events & WorkerEvtStop) {
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
