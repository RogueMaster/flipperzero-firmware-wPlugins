#include "seos_emulator_i.h"

#define TAG "SeosEmulator"

#define NAD_MASK 0x08

#define DESFIRE_CLA 0x90

static uint8_t select_header[] = {0x00, 0xa4, 0x04, 0x00};
static uint8_t standard_seos_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
static uint8_t MOBILE_SEOS_ADMIN_CARD[] =
    {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x2d, 0x00, 0x01, 0x01};
static uint8_t OPERATION_SELECTOR[] = {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x2f, 0x00, 0x01, 0x01};
static uint8_t OPERATION_SELECTOR_POST_RESET[] =
    {0xa0, 0x00, 0x00, 0x03, 0x82, 0x00, 0x31, 0x00, 0x01, 0x01};
static uint8_t DESFIRE_ISO_AID[] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};

static uint8_t FILE_NOT_FOUND[] = {0x6A, 0x82};
static uint8_t success[] = {0x90, 0x00};

static uint8_t select_adf_header[] = {0x80, 0xa5, 0x04, 0x00};
static uint8_t general_authenticate_1[] =
    {0x00, 0x87, 0x00, 0x01, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};
static uint8_t general_authenticate_1_response_header[] = {0x7c, 0x0a, 0x81, 0x08};
static uint8_t general_authenticate_2_header[] = {0x00, 0x87, 0x00, 0x01};
static uint8_t secure_messaging_header[] = {0x0c, 0xcb, 0x3f, 0xff};

SeosEmulator* seos_emulator_alloc(SeosCredential* credential) {
    SeosEmulator* seos_emulator = malloc(sizeof(SeosEmulator));
    memset(seos_emulator, 0, sizeof(SeosEmulator));

    // Using DES for greater compatibilty
    seos_emulator->params.cipher = TWO_KEY_3DES_CBC_MODE;
    seos_emulator->params.hash = SHA1;

    memset(seos_emulator->params.rndICC, 0x0d, sizeof(seos_emulator->params.rndICC));
    memset(seos_emulator->params.rNonce, 0x0c, sizeof(seos_emulator->params.rNonce));
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

void seos_emulator_select_aid(BitBuffer* tx_buffer, const uint8_t* aid, size_t aid_len) {
    FURI_LOG_D(TAG, "Select AID");
    bit_buffer_append_byte(tx_buffer, 0x6F); // FCI Template
    bit_buffer_append_byte(tx_buffer, 2 + aid_len); // length
    bit_buffer_append_byte(tx_buffer, 0x84); // DF Name
    bit_buffer_append_byte(tx_buffer, aid_len); // length
    bit_buffer_append_bytes(tx_buffer, aid, aid_len);
}

void seos_emulator_general_authenticate_1(BitBuffer* tx_buffer, AuthParameters params) {
    bit_buffer_append_bytes(
        tx_buffer,
        general_authenticate_1_response_header,
        sizeof(general_authenticate_1_response_header));
    bit_buffer_append_bytes(tx_buffer, params.rndICC, sizeof(params.rndICC));
}

// 0a00
// 00870001 2c7c 2a82 28 bbb4e9156136f27f687e2967865dfe812e33c95ddcf9294a4340d26da3e76db0220d1163c591e5b8 00
bool seos_emulator_general_authenticate_2(
    const uint8_t* buffer,
    size_t buffer_len,
    SeosCredential* credential,
    AuthParameters* params,
    BitBuffer* tx_buffer) {
    FURI_LOG_D(TAG, "seos_emulator_general_authenticate_2");
    UNUSED(buffer_len);

    uint8_t* rx_data = (uint8_t*)buffer;
    uint8_t* cryptogram = rx_data + sizeof(general_authenticate_2_header) + 5;
    size_t encrypted_len = 32;
    uint8_t* mac = cryptogram + encrypted_len;

    params->key_no = rx_data[3];

    uint8_t* master_key = SEOS_ADF1_READ;
    if(params->key_no == 0x02) {
        // Write keyslot
        master_key = SEOS_ADF1_WRITE;
    }

    if(credential->use_hardcoded) {
        memcpy(params->priv_key, credential->priv_key, sizeof(params->priv_key));
        memcpy(params->auth_key, credential->auth_key, sizeof(params->auth_key));
    } else {
        seos_worker_diversify_key(
            master_key,
            credential->diversifier,
            credential->diversifier_len,
            SEOS_ADF_OID,
            SEOS_ADF_OID_LEN,
            params->cipher,
            params->hash,
            params->key_no,
            true,
            params->priv_key);
        seos_worker_diversify_key(
            master_key,
            credential->diversifier,
            credential->diversifier_len,
            SEOS_ADF_OID,
            SEOS_ADF_OID_LEN,
            params->cipher,
            params->hash,
            params->key_no,
            false,
            params->auth_key);
    }

    uint8_t cmac[16];
    if(params->cipher == AES_128_CBC) {
        aes_cmac(params->auth_key, sizeof(params->auth_key), cryptogram, encrypted_len, cmac);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        des_cmac(params->auth_key, sizeof(params->auth_key), cryptogram, encrypted_len, cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
        return false;
    }

    if(memcmp(cmac, mac, SEOS_WORKER_CMAC_SIZE) != 0) {
        FURI_LOG_W(TAG, "Incorrect cryptogram mac %02x... vs %02x...", cmac[0], mac[0]);
        return false;
    }

    uint8_t clear[32];
    if(params->cipher == AES_128_CBC) {
        seos_worker_aes_decrypt(params->priv_key, encrypted_len, cryptogram, clear);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_decrypt(params->priv_key, encrypted_len, cryptogram, clear);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    size_t index = 0;
    memcpy(params->UID, clear + index, sizeof(params->UID));
    index += sizeof(params->UID);
    if(memcmp(clear + index, params->rndICC, sizeof(params->rndICC)) != 0) {
        FURI_LOG_W(TAG, "Incorrect rndICC returned");
        return false;
    }
    index += sizeof(params->rndICC);
    memcpy(params->cNonce, clear + index, sizeof(params->cNonce));
    index += sizeof(params->cNonce);

    // Construct response
    uint8_t response_header[] = {0x7c, 0x2a, 0x82, 0x28};
    memset(clear, 0, sizeof(clear));
    memset(cmac, 0, sizeof(cmac));
    index = 0;
    memcpy(clear + index, params->rndICC, sizeof(params->rndICC));
    index += sizeof(params->rndICC);
    memcpy(clear + index, params->UID, sizeof(params->UID));
    index += sizeof(params->UID);
    memcpy(clear + index, params->rNonce, sizeof(params->rNonce));
    index += sizeof(params->rNonce);

    uint8_t encrypted[32];
    if(params->cipher == AES_128_CBC) {
        seos_worker_aes_encrypt(params->priv_key, sizeof(clear), clear, encrypted);
        aes_cmac(params->auth_key, sizeof(params->auth_key), encrypted, sizeof(encrypted), cmac);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_encrypt(params->priv_key, sizeof(clear), clear, encrypted);
        des_cmac(params->auth_key, sizeof(params->auth_key), encrypted, sizeof(encrypted), cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    bit_buffer_append_bytes(tx_buffer, response_header, sizeof(response_header));
    bit_buffer_append_bytes(tx_buffer, encrypted, sizeof(encrypted));
    bit_buffer_append_bytes(tx_buffer, cmac, SEOS_WORKER_CMAC_SIZE);

    return true;
}

void seos_emulator_des_adf_payload(SeosCredential* credential, uint8_t* buffer) {
    // Synethic IV
    /// random bytes
    uint8_t rnd[4] = {0, 0, 0, 0};
    uint8_t cmac[8] = {0};
    /// cmac
    des_cmac(SEOS_ADF1_PRIV_MAC, sizeof(SEOS_ADF1_PRIV_MAC), rnd, sizeof(rnd), cmac);
    uint8_t iv[8];
    memcpy(iv + 0, rnd, sizeof(rnd));
    memcpy(iv + sizeof(rnd), cmac, sizeof(iv) - sizeof(rnd));

    // Copy IV to buffer because mbedtls_des3_crypt_cbc mutates it
    memcpy(buffer + 0, iv, sizeof(iv));

    uint8_t clear[0x30];
    memset(clear, 0, sizeof(clear));
    size_t index = 0;

    // OID
    clear[index++] = 0x06;
    clear[index++] = SEOS_ADF_OID_LEN, memcpy(clear + index, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    index += SEOS_ADF_OID_LEN;
    // diversifier
    clear[index++] = 0xcf;
    clear[index++] = credential->diversifier_len;
    memcpy(clear + index, credential->diversifier, credential->diversifier_len);
    index += credential->diversifier_len;

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    mbedtls_des3_set2key_enc(&ctx, SEOS_ADF1_PRIV_ENC);
    mbedtls_des3_crypt_cbc(
        &ctx, MBEDTLS_DES_ENCRYPT, sizeof(clear), iv, clear, buffer + sizeof(iv));
    mbedtls_des3_free(&ctx);
}

void seos_emulator_aes_adf_payload(SeosCredential* credential, uint8_t* buffer) {
    // Synethic IV
    /// random bytes
    uint8_t rnd[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t cmac[16] = {0};
    /// cmac
    aes_cmac(SEOS_ADF1_PRIV_MAC, sizeof(SEOS_ADF1_PRIV_MAC), rnd, sizeof(rnd), cmac);
    uint8_t iv[16];
    memcpy(iv + 0, rnd, sizeof(rnd));
    memcpy(iv + sizeof(rnd), cmac, sizeof(iv) - sizeof(rnd));

    // Copy IV to buffer because mbedtls_aes_crypt_cbc mutates it
    memcpy(buffer + 0, iv, sizeof(iv));

    uint8_t clear[0x30];
    memset(clear, 0, sizeof(clear));
    size_t index = 0;

    // OID
    clear[index++] = 0x06;
    clear[index++] = SEOS_ADF_OID_LEN;
    memcpy(clear + index, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    index += SEOS_ADF_OID_LEN;
    // diversifier
    clear[index++] = 0xcf;
    clear[index++] = credential->diversifier_len;
    memcpy(clear + index, credential->diversifier, credential->diversifier_len);
    index += credential->diversifier_len;

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, SEOS_ADF1_PRIV_ENC, sizeof(SEOS_ADF1_PRIV_ENC) * 8);
    mbedtls_aes_crypt_cbc(
        &ctx, MBEDTLS_AES_ENCRYPT, sizeof(clear), iv, clear, buffer + sizeof(iv));
    mbedtls_aes_free(&ctx);
}

bool seos_emulator_select_adf(
    const uint8_t* oid_list,
    size_t oid_list_len,
    AuthParameters* params,
    SeosCredential* credential,
    BitBuffer* tx_buffer) {
    FURI_LOG_D(TAG, "Select ADF");

    void* p = NULL;
    if(credential->adf_oid_len > 0) {
        p = memmem(oid_list, oid_list_len, credential->adf_oid, credential->adf_oid_len);
        if(p) {
            seos_log_buffer(TAG, "Select ADF OID(credential)", p, credential->adf_oid_len);

            if(credential->adf_response[0] == 0xCD) {
                FURI_LOG_I(TAG, "Using hardcoded ADF Response");
                // 4 byte cipher/hash
                // 2 byte cryptogram header
                // x bytes of cryptogram
                // 10 bytes for mac (2 byte header + 8 byte cmac)
                size_t adf_response_len = 4 + 2 + credential->adf_response[5] + 10;
                bit_buffer_append_bytes(tx_buffer, credential->adf_response, adf_response_len);

                params->cipher = credential->adf_response[2];
                params->hash = credential->adf_response[3];
                credential->use_hardcoded = true;
                return true;
            }
        }
    }
    // Next we try to match the ADF OID from the keys file
    p = memmem(oid_list, oid_list_len, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    if(p) {
        seos_log_buffer(TAG, "Select ADF OID(keys)", p, SEOS_ADF_OID_LEN);
    } else {
        return false;
    }

    size_t prefix_len = bit_buffer_get_size_bytes(tx_buffer);
    size_t des_cryptogram_length = 56;
    size_t aes_cryptogram_length = 64;
    uint8_t header[] = {0xcd, 0x02, params->cipher, params->hash};
    bit_buffer_append_bytes(tx_buffer, header, sizeof(header));

    // cryptogram
    // 06112b0601040181e438010102011801010202 cf 07 3d4c010c71cfa7 e2d0b41a00cc5e494c8d52b6e562592399fe614a
    uint8_t buffer[64];
    uint8_t cmac[16];
    memset(buffer, 0, sizeof(buffer));
    if(params->cipher == AES_128_CBC) {
        uint8_t cryptogram_prefix[] = {0x85, aes_cryptogram_length};
        bit_buffer_append_bytes(tx_buffer, cryptogram_prefix, sizeof(cryptogram_prefix));

        seos_emulator_aes_adf_payload(credential, buffer);
        bit_buffer_append_bytes(tx_buffer, buffer, aes_cryptogram_length);

        aes_cmac(
            SEOS_ADF1_PRIV_MAC,
            sizeof(SEOS_ADF1_PRIV_MAC),
            (uint8_t*)bit_buffer_get_data(tx_buffer) + prefix_len,
            bit_buffer_get_size_bytes(tx_buffer) - prefix_len,
            cmac);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        uint8_t cryptogram_prefix[] = {0x85, des_cryptogram_length};
        bit_buffer_append_bytes(tx_buffer, cryptogram_prefix, sizeof(cryptogram_prefix));

        seos_emulator_des_adf_payload(credential, buffer);
        bit_buffer_append_bytes(tx_buffer, buffer, des_cryptogram_length);

        des_cmac(
            SEOS_ADF1_PRIV_MAC,
            sizeof(SEOS_ADF1_PRIV_MAC),
            (uint8_t*)bit_buffer_get_data(tx_buffer) + prefix_len,
            bit_buffer_get_size_bytes(tx_buffer) - prefix_len,
            cmac);
    }

    uint8_t cmac_prefix[] = {0x8e, 0x08};
    bit_buffer_append_bytes(tx_buffer, cmac_prefix, sizeof(cmac_prefix));
    bit_buffer_append_bytes(tx_buffer, cmac, SEOS_WORKER_CMAC_SIZE);
    return true;
}

NfcCommand seos_worker_listener_inspect_reader(Seos* seos) {
    SeosEmulator* seos_emulator = seos->seos_emulator;
    BitBuffer* tx_buffer = seos_emulator->tx_buffer;
    NfcCommand ret = NfcCommandContinue;

    const uint8_t* rx_data = bit_buffer_get_data(seos_emulator->rx_buffer);
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
    // With PR #4242 ISO14443-4A command PCB is handled by firmware and not passed in rx buffer
    uint8_t offset = 0;
#else
    bool NAD = (rx_data[0] & NAD_MASK) == NAD_MASK;
    uint8_t offset = NAD ? 2 : 1;
#endif

    // + x to skip stuff before APDU
    const uint8_t* apdu = rx_data + offset;

    if(memcmp(apdu, select_header, sizeof(select_header)) == 0) {
        if(memcmp(
               apdu + sizeof(select_header) + 1, OPERATION_SELECTOR, sizeof(OPERATION_SELECTOR)) ==
           0) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR");
            uint8_t enableInspection[] = {
                0x6f, 0x08, 0x85, 0x06, 0x02, 0x01, 0x40, 0x02, 0x01, 0x00};

            bit_buffer_append_bytes(tx_buffer, enableInspection, sizeof(enableInspection));
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAIDSelected);
        } else {
            FURI_LOG_I(TAG, "Inspect mode: reject other AID");
            bit_buffer_append_bytes(tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        }
    } else if(apdu[0] == DESFIRE_CLA) {
        FURI_LOG_I(TAG, "Desfire command received: ignore");
    } else if(bit_buffer_get_size_bytes(seos_emulator->rx_buffer) > (size_t)(offset + 2)) {
        FURI_LOG_I(TAG, "NFC stop; %d bytes", bit_buffer_get_size_bytes(seos_emulator->rx_buffer));
        ret = NfcCommandStop;
    }

    return ret;
}

NfcCommand seos_worker_listener_process_message(Seos* seos) {
    SeosEmulator* seos_emulator = seos->seos_emulator;
    BitBuffer* tx_buffer = seos_emulator->tx_buffer;
    NfcCommand ret = NfcCommandContinue;

    const uint8_t* rx_data = bit_buffer_get_data(seos_emulator->rx_buffer);
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
    // With PR #4242 ISO14443-4A command PCB is handled by firmware and not passed in rx buffer
    uint8_t offset = 0;
#else
    bool NAD = (rx_data[0] & NAD_MASK) == NAD_MASK;
    uint8_t offset = NAD ? 2 : 1;
#endif

    // + x to skip stuff before APDU
    const uint8_t* apdu = rx_data + offset;

    if(memcmp(apdu, select_header, sizeof(select_header)) == 0) {
        seos_emulator->credential->use_hardcoded = false;
        if(memcmp(apdu + sizeof(select_header) + 1, standard_seos_aid, sizeof(standard_seos_aid)) ==
           0) {
            seos_emulator_select_aid(
                seos_emulator->tx_buffer,
                apdu + sizeof(select_header) + 1,
                sizeof(standard_seos_aid));
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAIDSelected);
        } else if(
            memcmp(
                apdu + sizeof(select_header) + 1,
                OPERATION_SELECTOR_POST_RESET,
                sizeof(OPERATION_SELECTOR_POST_RESET)) == 0) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR_POST_RESET");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        } else if(
            memcmp(
                apdu + sizeof(select_header) + 1,
                OPERATION_SELECTOR,
                sizeof(OPERATION_SELECTOR)) == 0) {
            FURI_LOG_I(TAG, "OPERATION_SELECTOR");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        } else if(
            memcmp(
                apdu + sizeof(select_header) + 1,
                MOBILE_SEOS_ADMIN_CARD,
                sizeof(MOBILE_SEOS_ADMIN_CARD)) == 0) {
            FURI_LOG_I(TAG, "MOBILE_SEOS_ADMIN_CARD");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        } else if(
            memcmp(apdu + sizeof(select_header) + 1, DESFIRE_ISO_AID, sizeof(DESFIRE_ISO_AID)) ==
            0) {
            FURI_LOG_I(TAG, "DESFIRE_ISO_AID");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        } else {
            seos_log_bitbuffer(TAG, "Reject unknown AID", seos_emulator->rx_buffer);
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        }
    } else if(memcmp(apdu, select_adf_header, sizeof(select_adf_header)) == 0) {
        // +1 to skip APDU length byte
        const uint8_t* oid_list = apdu + sizeof(select_adf_header) + 1;
        size_t oid_list_len = apdu[sizeof(select_adf_header)];

        if(seos_emulator_select_adf(
               oid_list,
               oid_list_len,
               &seos_emulator->params,
               seos_emulator->credential,
               seos_emulator->tx_buffer)) {
            view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventADFMatched);
        } else {
            FURI_LOG_W(TAG, "Failed to match any ADF OID");
            bit_buffer_append_bytes(
                seos_emulator->tx_buffer, (uint8_t*)FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
        }
    } else if(memcmp(apdu, general_authenticate_1, sizeof(general_authenticate_1)) == 0) {
        seos_emulator_general_authenticate_1(seos_emulator->tx_buffer, seos_emulator->params);
    } else if(memcmp(apdu, general_authenticate_2_header, sizeof(general_authenticate_2_header)) == 0) {
        if(!seos_emulator_general_authenticate_2(
               apdu,
               bit_buffer_get_size_bytes(seos_emulator->rx_buffer),
               seos_emulator->credential,
               &seos_emulator->params,
               seos_emulator->tx_buffer)) {
            FURI_LOG_W(TAG, "Failure in General Authenticate 2");
            ret = NfcCommandStop;
            return ret;
        }
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventAuthenticated);
        // Prepare for future communication
        seos_emulator->secure_messaging = secure_messaging_alloc(&seos_emulator->params);
    } else if(memcmp(apdu, secure_messaging_header, sizeof(secure_messaging_header)) == 0) {
        uint8_t request_sio[] = {0x5c, 0x02, 0xff, 0x00};

        if(seos_emulator->secure_messaging) {
            FURI_LOG_D(TAG, "Unwrap secure message");

            // 0b00 0ccb3fff 16 8508fa8395d30de4e8e097008e085da7edbd833b002d00
            // Ignore 2 iso frame bytes
            size_t bytes_to_ignore = offset;
            BitBuffer* tmp = bit_buffer_alloc(bit_buffer_get_size_bytes(seos_emulator->rx_buffer));
            bit_buffer_append_bytes(
                tmp,
                bit_buffer_get_data(seos_emulator->rx_buffer) + bytes_to_ignore,
                bit_buffer_get_size_bytes(seos_emulator->rx_buffer) - bytes_to_ignore);

            seos_log_bitbuffer(TAG, "NFC received(wrapped)", tmp);
            secure_messaging_unwrap_apdu(seos_emulator->secure_messaging, tmp);
            seos_log_bitbuffer(TAG, "NFC received(clear)", tmp);

            const uint8_t* message = bit_buffer_get_data(tmp);
            if(memcmp(message, request_sio, sizeof(request_sio)) == 0) {
                view_dispatcher_send_custom_event(
                    seos->view_dispatcher, SeosCustomEventSIORequested);
                BitBuffer* sio_file = bit_buffer_alloc(128);
                bit_buffer_append_bytes(sio_file, message + 2, 2); // fileId
                bit_buffer_append_byte(sio_file, seos_emulator->credential->sio_len);
                bit_buffer_append_bytes(
                    sio_file, seos_emulator->credential->sio, seos_emulator->credential->sio_len);

                seos_log_bitbuffer(TAG, "NFC send(clear)", sio_file);
                secure_messaging_wrap_rapdu(
                    seos_emulator->secure_messaging,
                    (uint8_t*)bit_buffer_get_data(sio_file),
                    bit_buffer_get_size_bytes(sio_file),
                    tx_buffer);

                bit_buffer_free(sio_file);
            }

            bit_buffer_free(tmp);
        } else {
            uint8_t no_sm[] = {0x69, 0x88};
            bit_buffer_append_bytes(tx_buffer, no_sm, sizeof(no_sm));
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
#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        // With PR #4242 ISO14443-4A command PCB is handled by firmware and not passed in rx buffer
        uint8_t offset = 0;
        UNUSED(rx_data);
#else
        bool NAD = (rx_data[0] & NAD_MASK) == NAD_MASK;
        uint8_t offset = NAD ? 2 : 1;
#endif

        if(bit_buffer_get_size_bytes(iso14443_4a_event->data->buffer) == offset) {
            FURI_LOG_I(TAG, "No contents in frame");
            break;
        }

        seos_log_bitbuffer(TAG, "NFC received", seos_emulator->rx_buffer);

#if __has_include(<lib/nfc/protocols/type_4_tag/type_4_tag.h>)
        // With PR #4242 ISO14443-4A response PCB is handled by firmware and not necessary in tx buffer
#else
        // Some ISO14443a framing I need to figure out
        bit_buffer_append_bytes(tx_buffer, rx_data, offset);
#endif

        if(seos->flow_mode == FLOW_CRED) {
            ret = seos_worker_listener_process_message(seos);
        } else if(seos->flow_mode == FLOW_INSPECT) {
            ret = seos_worker_listener_inspect_reader(seos);
        }

        if(bit_buffer_get_size_bytes(seos_emulator->tx_buffer) >
           offset) { // contents belong iso framing

            uint8_t* statusword = (uint8_t*)bit_buffer_get_data(tx_buffer) +
                                  bit_buffer_get_size_bytes(tx_buffer) - sizeof(uint16_t);
            if(memcmp(success, statusword, sizeof(success)) == 0) {
                // no-op
            } else if(memcmp(FILE_NOT_FOUND, statusword, sizeof(FILE_NOT_FOUND)) == 0) {
                // no-op
            } else {
                bit_buffer_append_bytes(tx_buffer, success, sizeof(success));
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
