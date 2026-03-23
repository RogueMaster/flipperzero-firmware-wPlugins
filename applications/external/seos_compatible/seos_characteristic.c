#include "seos_characteristic_i.h"

#define TAG "SeosCharacteristic"

static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t general_authenticate_1[] =
    {0x00, 0x87, 0x00, 0x01, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};
static uint8_t cd02[] = {0xcd, 0x02};

static uint8_t ga1_response[] = {0x7c, 0x0a, 0x81, 0x08};

// Emulation
static uint8_t success[] = {0x90, 0x00};
static uint8_t file_not_found[] = {0x6A, 0x82};

static uint8_t select_header[] = {0x00, 0xa4, 0x04, 0x00};
static uint8_t select_adf_header[] = {0x80, 0xa5, 0x04, 0x00};
static uint8_t general_authenticate_2_header[] = {0x00, 0x87, 0x00, 0x01};
static uint8_t secure_messaging_header[] = {0x0c, 0xcb, 0x3f, 0xff};

SeosCharacteristic* seos_characteristic_alloc(Seos* seos) {
    SeosCharacteristic* seos_characteristic = malloc(sizeof(SeosCharacteristic));
    memset(seos_characteristic, 0, sizeof(SeosCharacteristic));
    seos_characteristic->seos = seos;
    seos_characteristic->credential = seos->credential;

    seos_characteristic->phase = SELECT_AID;
    seos_characteristic->secure_messaging = NULL;

    seos_characteristic->params.key_no = 1;
    memset(seos_characteristic->params.cNonce, 0x0c, sizeof(seos_characteristic->params.cNonce));
    memset(seos_characteristic->params.UID, 0x0d, sizeof(seos_characteristic->params.UID));

    seos_characteristic->seos_att = seos_att_alloc(seos);

    seos_att_set_on_subscribe_callback(
        seos_characteristic->seos_att, seos_characteristic_on_subscribe, seos_characteristic);

    seos_att_set_write_request_callback(
        seos_characteristic->seos_att, seos_characteristic_write_request, seos_characteristic);

    seos_characteristic->rx_buffer = bit_buffer_alloc(128); // TODO: MTU

    return seos_characteristic;
}

void seos_characteristic_free(SeosCharacteristic* seos_characteristic) {
    furi_assert(seos_characteristic);
    seos_att_free(seos_characteristic->seos_att);
    bit_buffer_free(seos_characteristic->rx_buffer);
    free(seos_characteristic);
}

void seos_characteristic_start(SeosCharacteristic* seos_characteristic, FlowMode mode) {
    seos_characteristic->flow_mode = mode;
    if(seos_characteristic->flow_mode == FLOW_CRED) {
        seos_characteristic->params.key_no = 0;
        seos_characteristic->params.cipher = TWO_KEY_3DES_CBC_MODE;
        seos_characteristic->params.hash = SHA1;

        memset(
            seos_characteristic->params.rndICC, 0x0d, sizeof(seos_characteristic->params.rndICC));
        memset(
            seos_characteristic->params.rNonce, 0x0c, sizeof(seos_characteristic->params.rNonce));
        memset(seos_characteristic->params.UID, 0x00, sizeof(seos_characteristic->params.UID));
        memset(
            seos_characteristic->params.cNonce, 0x00, sizeof(seos_characteristic->params.cNonce));
    }
    seos_att_start(seos_characteristic->seos_att, BLE_PERIPHERAL, mode);
}

void seos_characteristic_stop(SeosCharacteristic* seos_characteristic) {
    seos_att_stop(seos_characteristic->seos_att);
}

void seos_characteristic_reader_flow(
    SeosCharacteristic* seos_characteristic,
    BitBuffer* attribute_value,
    BitBuffer* payload) {
    const uint8_t* data = bit_buffer_get_data(attribute_value);
    const uint8_t* rx_data = data + 1; // Match name to nfc version for easier copying

    // 022f20180014000400
    // 520c00
    // c0 6f0c840a a0000004400001010001
    // 9000
    if(memcmp(data + 5, standard_seos_aid, sizeof(standard_seos_aid)) == 0) { // response to select
        FURI_LOG_I(TAG, "Select ADF");
        uint8_t select_adf_header[] = {
            0x80, 0xa5, 0x04, 0x00, (uint8_t)SEOS_ADF_OID_LEN + 2, 0x06, (uint8_t)SEOS_ADF_OID_LEN};

        bit_buffer_append_bytes(payload, select_adf_header, sizeof(select_adf_header));
        bit_buffer_append_bytes(payload, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
        seos_characteristic->phase = SELECT_ADF;
    } else if(memcmp(data + 1, cd02, sizeof(cd02)) == 0) {
        if(seos_reader_select_adf_response(
               attribute_value, 1, seos_characteristic->credential, &seos_characteristic->params)) {
            // Craft response
            general_authenticate_1[3] = seos_characteristic->params.key_no;
            bit_buffer_append_bytes(
                payload, general_authenticate_1, sizeof(general_authenticate_1));
            seos_characteristic->phase = GENERAL_AUTHENTICATION_1;
        }
    } else if(memcmp(data + 1, ga1_response, sizeof(ga1_response)) == 0) {
        memcpy(seos_characteristic->params.rndICC, data + 5, 8);

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
    } else if(rx_data[0] == 0x7C && rx_data[2] == 0x82) { // ga2 response
        if(rx_data[3] == 40) {
            if(!seos_reader_verify_cryptogram(&seos_characteristic->params, rx_data + 4)) {
                FURI_LOG_W(TAG, "Card cryptogram failed verification");
                return;
            }
            FURI_LOG_I(TAG, "Authenticated");
            view_dispatcher_send_custom_event(
                seos_characteristic->seos->view_dispatcher, SeosCustomEventAuthenticated);
        } else {
            FURI_LOG_W(TAG, "Unhandled card cryptogram size %d", rx_data[3]);
        }

        seos_characteristic->secure_messaging =
            secure_messaging_alloc(&seos_characteristic->params);

        SecureMessaging* secure_messaging = seos_characteristic->secure_messaging;

        uint8_t message[] = {0x5c, 0x02, 0xff, 0x00};
        secure_messaging_wrap_apdu(
            secure_messaging,
            message,
            sizeof(message),
            secure_messaging_header,
            sizeof(secure_messaging_header),
            payload);
        seos_characteristic->phase = REQUEST_SIO;
        view_dispatcher_send_custom_event(
            seos_characteristic->seos->view_dispatcher, SeosCustomEventSIORequested);
    } else if(seos_characteristic->phase == REQUEST_SIO) {
        SecureMessaging* secure_messaging = seos_characteristic->secure_messaging;

        BitBuffer* rx_buffer = bit_buffer_alloc(bit_buffer_get_size_bytes(attribute_value) - 1);
        bit_buffer_append_bytes(
            rx_buffer, rx_data, bit_buffer_get_size_bytes(attribute_value) - 1);
        seos_log_bitbuffer(TAG, "BLE response(wrapped)", rx_buffer);
        secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer);
        seos_log_bitbuffer(TAG, "BLE response(clear)", rx_buffer);

        // Skip fileId
        seos_characteristic->credential->sio_len = bit_buffer_get_byte(rx_buffer, 2);
        if(seos_characteristic->credential->sio_len >
           sizeof(seos_characteristic->credential->sio)) {
            FURI_LOG_W(TAG, "SIO too long to save");
            return;
        }
        memcpy(
            seos_characteristic->credential->sio,
            bit_buffer_get_data(rx_buffer) + 3,
            seos_characteristic->credential->sio_len);
        FURI_LOG_I(TAG, "SIO Captured, %d bytes", seos_characteristic->credential->sio_len);

        Seos* seos = seos_characteristic->seos;
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerSuccess);
        bit_buffer_free(rx_buffer);

        seos_characteristic->phase = SELECT_AID;
    } else if(data[0] == 0xe1) {
        //ignore
    } else {
        FURI_LOG_W(TAG, "No match for write request");
    }
}

void seos_characteristic_cred_flow(
    SeosCharacteristic* seos_characteristic,
    BitBuffer* attribute_value,
    BitBuffer* payload) {
    const uint8_t* data = bit_buffer_get_data(attribute_value);
    const uint8_t* apdu = data + 1; // Match name to nfc version for easier copying

    if(memcmp(apdu, select_header, sizeof(select_header)) == 0) {
        if(memcmp(apdu + sizeof(select_header) + 1, standard_seos_aid, sizeof(standard_seos_aid)) ==
           0) {
            seos_emulator_select_aid(
                payload, apdu + sizeof(select_header) + 1, sizeof(standard_seos_aid));
            bit_buffer_append_bytes(payload, (uint8_t*)success, sizeof(success));
        } else {
            bit_buffer_append_bytes(payload, (uint8_t*)file_not_found, sizeof(file_not_found));
        }
    } else if(memcmp(apdu, select_adf_header, sizeof(select_adf_header)) == 0) {
        const uint8_t* oid_list = apdu + sizeof(select_adf_header) + 1;
        size_t oid_list_len = apdu[sizeof(select_adf_header)];

        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_characteristic->params,
               seos_characteristic->credential,
               payload)) {
            bit_buffer_append_bytes(payload, (uint8_t*)success, sizeof(success));
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
        }

    } else if(memcmp(apdu, general_authenticate_1, sizeof(general_authenticate_1)) == 0) {
        seos_emulator_general_authenticate_1(payload, seos_characteristic->params);
        bit_buffer_append_bytes(payload, (uint8_t*)success, sizeof(success));
    } else if(memcmp(apdu, general_authenticate_2_header, sizeof(general_authenticate_2_header)) == 0) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               bit_buffer_get_size_bytes(attribute_value),
               seos_characteristic->credential,
               &seos_characteristic->params,
               payload)) {
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
        } else {
            bit_buffer_append_bytes(payload, (uint8_t*)success, sizeof(success));
        }

        view_dispatcher_send_custom_event(
            seos_characteristic->seos->view_dispatcher, SeosCustomEventAuthenticated);
        // Prepare for future communication
        seos_characteristic->secure_messaging =
            secure_messaging_alloc(&seos_characteristic->params);
    } else if(memcmp(apdu, secure_messaging_header, sizeof(secure_messaging_header)) == 0) {
        uint8_t request_sio[] = {0x5c, 0x02, 0xff, 0x00};

        if(seos_characteristic->secure_messaging) {
            FURI_LOG_D(TAG, "Unwrap secure message");

            // c0 0ccb3fff 16 8508fa8395d30de4e8e097008e085da7edbd833b002d00
            // Ignore 1 BLE_START byte
            size_t bytes_to_ignore = 1;
            BitBuffer* tmp = bit_buffer_alloc(bit_buffer_get_size_bytes(attribute_value));
            bit_buffer_append_bytes(
                tmp,
                bit_buffer_get_data(attribute_value) + bytes_to_ignore,
                bit_buffer_get_size_bytes(attribute_value) - bytes_to_ignore);

            seos_log_bitbuffer(TAG, "received(wrapped)", tmp);
            secure_messaging_unwrap_apdu(seos_characteristic->secure_messaging, tmp);
            seos_log_bitbuffer(TAG, "received(clear)", tmp);

            const uint8_t* message = bit_buffer_get_data(tmp);
            if(memcmp(message, request_sio, sizeof(request_sio)) == 0) {
                view_dispatcher_send_custom_event(
                    seos_characteristic->seos->view_dispatcher, SeosCustomEventSIORequested);

                BitBuffer* sio_file = bit_buffer_alloc(128);
                bit_buffer_append_bytes(sio_file, message + 2, 2); // fileId
                bit_buffer_append_byte(sio_file, seos_characteristic->credential->sio_len);
                bit_buffer_append_bytes(
                    sio_file,
                    seos_characteristic->credential->sio,
                    seos_characteristic->credential->sio_len);

                secure_messaging_wrap_rapdu(
                    seos_characteristic->secure_messaging,
                    (uint8_t*)bit_buffer_get_data(sio_file),
                    bit_buffer_get_size_bytes(sio_file),
                    payload);
                bit_buffer_append_bytes(payload, (uint8_t*)success, sizeof(success));

                bit_buffer_free(sio_file);
            }

            bit_buffer_free(tmp);
        } else {
            uint8_t no_sm[] = {0x69, 0x88};
            bit_buffer_append_bytes(payload, no_sm, sizeof(no_sm));
        }
    } else if(data[0] == 0xe1) {
        // ignore
    } else {
        FURI_LOG_W(TAG, "no match for attribute_value");
    }
}

void seos_characteristic_att_notify_chunk(SeosAtt* seos_att, uint16_t handle, BitBuffer* payload) {
    BitBuffer* tx = bit_buffer_alloc(1 + BLE_CHUNK_SIZE);

    const uint8_t* data = bit_buffer_get_data(payload);
    const uint16_t size = bit_buffer_get_size_bytes(payload);

    uint16_t num_chunks = size / BLE_CHUNK_SIZE;
    if(size % BLE_CHUNK_SIZE) num_chunks++;

    for(uint16_t i = 0; i < num_chunks; i++) {
        uint8_t flags = 0;
        if(i == 0) flags |= BLE_FLAG_SOM;
        if(i == num_chunks - 1) flags |= BLE_FLAG_EOM;
        // Add number of remaining chunks to lower nybble
        flags |= (num_chunks - 1 - i) & 0x0F;

        // Find number of bytes left to send
        uint8_t chunk_size = size - (i * BLE_CHUNK_SIZE);
        // Limit to maximum chunk size
        chunk_size = chunk_size > BLE_CHUNK_SIZE ? BLE_CHUNK_SIZE : chunk_size;

        // Combine and send
        bit_buffer_reset(tx);
        bit_buffer_append_byte(tx, flags);
        bit_buffer_append_bytes(tx, &data[i * BLE_CHUNK_SIZE], chunk_size);
        seos_att_notify(seos_att, handle, tx);
    }
    bit_buffer_free(tx);
}

void seos_characteristic_write_request(void* context, BitBuffer* attribute_value) {
    SeosCharacteristic* seos_characteristic = (SeosCharacteristic*)context;
    seos_log_bitbuffer(TAG, "write request", attribute_value);

    BitBuffer* payload = bit_buffer_alloc(128); // TODO: MTU
    const uint8_t* data = bit_buffer_get_data(attribute_value);
    const size_t len = bit_buffer_get_size_bytes(attribute_value);

    uint8_t flags = data[0];

    // Check for error flag
    if((flags & BLE_FLAG_ERR) == BLE_FLAG_ERR) {
        seos_log_buffer(TAG, "Received error response", (uint8_t*)(data + 1), len - 1);
        return;
    }

    // Check for start-of-message flag
    if((flags & BLE_FLAG_SOM) == BLE_FLAG_SOM) {
        bit_buffer_reset(seos_characteristic->rx_buffer);
    } else {
        if(bit_buffer_get_size_bytes(seos_characteristic->rx_buffer) == 0) {
            FURI_LOG_W(TAG, "Expected start of BLE packet");
            return;
        }
    }

    bit_buffer_append_bytes(seos_characteristic->rx_buffer, data + 1, len - 1);

    // Only parse if end-of-message flag found
    if((flags & BLE_FLAG_EOM) != BLE_FLAG_EOM) return;

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
