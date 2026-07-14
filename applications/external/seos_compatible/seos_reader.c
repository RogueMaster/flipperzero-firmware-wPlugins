#include "seos_reader_i.h"

#define TAG "SeosReader"

static uint8_t success[] = {0x90, 0x00};
static uint8_t select[] =
    {0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00};
static uint8_t SEOS_APPLET_FCI[] =
    {0x6F, 0x0C, 0x84, 0x0A, 0xA0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};

static uint8_t general_authenticate_1[] =
    {0x00, 0x87, 0x00, 0x01, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};

SeosReader* seos_reader_alloc(SeosCredential* credential, Iso14443_4aPoller* iso14443_4a_poller) {
    SeosReader* seos_reader = malloc(sizeof(SeosReader));
    memset(seos_reader, 0, sizeof(SeosReader));
    seos_reader->params.key_no = 1;
    seos_reader->secure_messaging = NULL;
    memset(seos_reader->params.cNonce, 0x0c, sizeof(seos_reader->params.cNonce));
    memset(seos_reader->params.UID, 0x0d, sizeof(seos_reader->params.UID));

    seos_reader->credential = credential;
    seos_reader->iso14443_4a_poller = iso14443_4a_poller;

    seos_reader->tx_buffer = bit_buffer_alloc(SEOS_WORKER_MAX_BUFFER_SIZE);
    seos_reader->rx_buffer = bit_buffer_alloc(SEOS_WORKER_MAX_BUFFER_SIZE);

    return seos_reader;
}

void seos_reader_free(SeosReader* seos_reader) {
    furi_assert(seos_reader);
    bit_buffer_free(seos_reader->tx_buffer);
    bit_buffer_free(seos_reader->rx_buffer);
    if(seos_reader->secure_messaging) {
        secure_messaging_free(seos_reader->secure_messaging);
    }
    free(seos_reader);
}

bool seos_reader_request_sio(SeosReader* seos_reader) {
    SecureMessaging* secure_messaging = seos_reader->secure_messaging;
    furi_assert(secure_messaging);
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;
    Iso14443_4aError error;

    uint8_t apdu_header[] = {0x0c, 0xcb, 0x3f, 0xff};

    uint8_t message[] = {0x5c, 0x02, 0xff, 0x00};
    secure_messaging_wrap_apdu(
        secure_messaging, message, sizeof(message), apdu_header, sizeof(apdu_header), tx_buffer);

    seos_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return false;
    }
    bit_buffer_reset(tx_buffer);

    seos_log_bitbuffer(TAG, "NFC response(wrapped)", rx_buffer);
    secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer);
    seos_log_bitbuffer(TAG, "NFC response(clear)", rx_buffer);

    // Skip fileId
    seos_reader->credential->sio_len = bit_buffer_get_byte(rx_buffer, 2);
    if(seos_reader->credential->sio_len > sizeof(seos_reader->credential->sio)) {
        FURI_LOG_W(TAG, "SIO too long to save");
        return false;
    }
    memcpy(
        seos_reader->credential->sio,
        bit_buffer_get_data(rx_buffer) + 3,
        seos_reader->credential->sio_len);

    return true;
}

bool seos_reader_write_sio(SeosReader* seos_reader) {
    SecureMessaging* secure_messaging = seos_reader->secure_messaging;
    furi_assert(secure_messaging);
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;
    Iso14443_4aError error;

    uint8_t apdu_header[] = {0x0c, 0xdb, 0x3f, 0xff};

    uint8_t message_prefix[] = {0xff, 0x00, seos_reader->credential->sio_len};
    uint8_t message[sizeof(message_prefix) + seos_reader->credential->sio_len];
    memcpy(message, message_prefix, sizeof(message_prefix));
    memcpy(
        message + sizeof(message_prefix),
        seos_reader->credential->sio,
        seos_reader->credential->sio_len);
    seos_log_buffer(TAG, "NFC transmit(clear)", message, sizeof(message));
    secure_messaging_wrap_apdu(
        secure_messaging, message, sizeof(message), apdu_header, sizeof(apdu_header), tx_buffer);

    seos_log_bitbuffer(TAG, "NFC transmit(wrapped)", tx_buffer);
    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return false;
    }
    bit_buffer_reset(tx_buffer);

    seos_log_bitbuffer(TAG, "NFC response", rx_buffer);
    if(memcmp(
           bit_buffer_get_data(rx_buffer) + bit_buffer_get_size_bytes(rx_buffer) - sizeof(success),
           success,
           sizeof(success)) != 0) {
        FURI_LOG_W(TAG, "Non-success response");
        return false;
    }

    return true;
}

void seos_reader_generate_cryptogram(
    SeosCredential* credential,
    AuthParameters* params,
    uint8_t* cryptogram) {
    uint8_t* master_key = SEOS_ADF1_READ;
    if(params->key_no == 0x02) {
        // Write keyslot
        master_key = SEOS_ADF1_WRITE;
    }

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

    uint8_t clear[32];
    memset(clear, 0, sizeof(clear));
    size_t index = 0;
    memcpy(clear + index, params->UID, sizeof(params->UID));
    index += sizeof(params->UID);
    memcpy(clear + index, params->rndICC, sizeof(params->rndICC));
    index += sizeof(params->rndICC);
    memcpy(clear + index, params->cNonce, sizeof(params->cNonce));
    index += sizeof(params->cNonce);

    uint8_t cmac[16];
    if(params->cipher == AES_128_CBC) {
        seos_worker_aes_encrypt(params->priv_key, sizeof(clear), clear, cryptogram);

        aes_cmac(params->auth_key, sizeof(params->auth_key), cryptogram, index, cmac);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_encrypt(params->priv_key, sizeof(clear), clear, cryptogram);

        des_cmac(params->auth_key, sizeof(params->auth_key), cryptogram, index, cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }
    memcpy(cryptogram + sizeof(clear), cmac, SEOS_WORKER_CMAC_SIZE);
}

bool seos_reader_verify_cryptogram(AuthParameters* params, const uint8_t* cryptogram) {
    // cryptogram is 40 bytes: 32 byte encrypted + 8 byte cmac
    size_t encrypted_len = 32;
    uint8_t* mac = (uint8_t*)cryptogram + encrypted_len;
    uint8_t cmac[16];
    if(params->cipher == AES_128_CBC) {
        aes_cmac(
            params->auth_key, sizeof(params->auth_key), (uint8_t*)cryptogram, encrypted_len, cmac);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        des_cmac(
            params->auth_key, sizeof(params->auth_key), (uint8_t*)cryptogram, encrypted_len, cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    if(memcmp(cmac, mac, SEOS_WORKER_CMAC_SIZE) != 0) {
        FURI_LOG_W(TAG, "Incorrect cryptogram mac %02x... vs %02x...", cmac[0], mac[0]);
        return false;
    }

    uint8_t clear[32];
    memset(clear, 0, sizeof(clear));
    if(params->cipher == AES_128_CBC) {
        seos_worker_aes_decrypt(params->priv_key, encrypted_len, cryptogram, clear);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_decrypt(params->priv_key, encrypted_len, cryptogram, clear);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    // rndICC[8], UID[8], rNonce[16]
    uint8_t* rndICC = clear;
    if(memcmp(rndICC, params->rndICC, sizeof(params->rndICC)) != 0) {
        FURI_LOG_W(TAG, "Incorrect rndICC returned");
        return false;
    }
    uint8_t* UID = clear + 8;
    if(memcmp(UID, params->UID, sizeof(params->UID)) != 0) {
        FURI_LOG_W(TAG, "Incorrect UID returned");
        return false;
    }

    memcpy(params->rNonce, clear + 8 + 8, sizeof(params->rNonce));
    return true;
}

NfcCommand seos_reader_select_aid(SeosReader* seos_reader) {
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aError error;

    bit_buffer_append_bytes(tx_buffer, select, sizeof(select));
    seos_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    seos_log_bitbuffer(TAG, "NFC response", rx_buffer);

    // TODO: validate response

    if(memcmp(
           bit_buffer_get_data(rx_buffer) + bit_buffer_get_size_bytes(rx_buffer) - sizeof(success),
           success,
           sizeof(success)) != 0) {
        FURI_LOG_W(TAG, "Non-success response");
        return NfcCommandStop;
    }

    if(memcmp(bit_buffer_get_data(rx_buffer), SEOS_APPLET_FCI, sizeof(SEOS_APPLET_FCI)) != 0) {
        FURI_LOG_W(TAG, "Unexpected select AID response");
        return NfcCommandStop;
    }

    return ret;
}

NfcCommand seos_reader_select_adf(SeosReader* seos_reader) {
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aError error;

    uint8_t select_adf_header[] = {
        0x80, 0xa5, 0x04, 0x00, (uint8_t)SEOS_ADF_OID_LEN + 2, 0x06, (uint8_t)SEOS_ADF_OID_LEN};

    bit_buffer_append_bytes(tx_buffer, select_adf_header, sizeof(select_adf_header));
    bit_buffer_append_bytes(tx_buffer, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    bit_buffer_append_byte(tx_buffer, 0x00); // Le

    seos_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    seos_log_bitbuffer(TAG, "NFC response", rx_buffer);
    if(memcmp(
           bit_buffer_get_data(rx_buffer) + bit_buffer_get_size_bytes(rx_buffer) - sizeof(success),
           success,
           sizeof(success)) != 0) {
        FURI_LOG_W(TAG, "Non-success response");
        return NfcCommandStop;
    }

    bit_buffer_reset(tx_buffer);
    return ret;
}

bool seos_reader_select_adf_response(
    BitBuffer* rx_buffer,
    size_t offset,
    SeosCredential* credential,
    AuthParameters* params) {
    seos_log_bitbuffer(TAG, "response", rx_buffer);

    // cd 02 0206
    // 85 38 41c01a89db89aecf 4b35b4f18dc4045b2a3d65cdd1c1944e8c8548f786e6c51128a5c8546a27120a7e44ba0f4cd7218a026ea1a73a9211a9
    // 8e 08 20f830009042cb85

    uint8_t expected_header[] = {0xcd, 0x02};
    if(bit_buffer_get_size_bytes(rx_buffer) < sizeof(expected_header)) {
        FURI_LOG_W(TAG, "Invalid response length");
        return false;
    }
    // handle when the buffer starts with other stuff
    const uint8_t* rx_data = bit_buffer_get_data(rx_buffer) + offset;
    if(memcmp(rx_data, expected_header, sizeof(expected_header)) != 0) {
        FURI_LOG_W(TAG, "Invalid response");
        return false;
    }
    params->cipher = rx_data[2];
    params->hash = rx_data[3];

    memset(credential->adf_response, 0, sizeof(credential->adf_response));
    size_t response_length = bit_buffer_get_size_bytes(rx_buffer) - offset - sizeof(success);
    if(response_length > sizeof(credential->adf_response)) {
        FURI_LOG_W(
            TAG,
            "adf_response too large %zu > %zu",
            response_length,
            sizeof(credential->adf_response));
        response_length = sizeof(credential->adf_response);
    }
    memcpy(credential->adf_response, rx_data, response_length);

    size_t bufLen = 0;
    uint8_t clear[0x40];
    memset(clear, 0, sizeof(clear));

    // Copy IV because mbedtls methods mutate it
    if(params->cipher == AES_128_CBC) {
        uint8_t iv[16];
        memcpy(iv, rx_data + 6, sizeof(iv));
        bufLen = rx_data[5] - sizeof(iv);
        uint8_t* enc = (uint8_t*)rx_data + 6 + sizeof(iv);

        mbedtls_aes_context ctx;
        mbedtls_aes_init(&ctx);
        mbedtls_aes_setkey_dec(&ctx, SEOS_ADF1_PRIV_ENC, sizeof(SEOS_ADF1_PRIV_ENC) * 8);
        mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, bufLen, iv, enc, clear);
        mbedtls_aes_free(&ctx);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        uint8_t iv[8];
        memcpy(iv, rx_data + 6, sizeof(iv));
        bufLen = rx_data[5] - sizeof(iv);
        uint8_t* enc = (uint8_t*)rx_data + 6 + sizeof(iv);

        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_dec(&ctx, SEOS_ADF1_PRIV_ENC);
        mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, bufLen, iv, enc, clear);
        mbedtls_des3_free(&ctx);
    }
    seos_log_buffer(TAG, "clear", clear, sizeof(clear));

    // 06112b0601040181e438010102011801010202 cf 07 3d4c010c71cfa7 e2d0b41a00cc5e494c8d52b6e562592399fe614a
    if(clear[0] != 0x06) {
        FURI_LOG_W(TAG, "Missing expected 0x06 at start of clear");
        return false;
    }
    size_t oidLen = clear[1];
    if(clear[2 + oidLen] != 0xCF) {
        FURI_LOG_W(TAG, "Missing expected 0xCF after OID");
        return false;
    }
    credential->diversifier_len = clear[2 + oidLen + 1];
    if(credential->diversifier_len > sizeof(credential->diversifier)) {
        FURI_LOG_W(TAG, "diversifier too large");
        return false;
    }

    uint8_t* diversifier = clear + 2 + oidLen + 2;
    memcpy(credential->diversifier, diversifier, credential->diversifier_len);

    char display[SEOS_WORKER_MAX_BUFFER_SIZE * 2 + 1];
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < credential->diversifier_len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", diversifier[i]);
    }
    FURI_LOG_D(TAG, "diversifier: %s", display);

    return true;
}

NfcCommand seos_reader_general_authenticate_1(SeosReader* seos_reader) {
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;
    FURI_LOG_D(TAG, "General Authenticate 1 with key no %d", seos_reader->params.key_no);

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aError error;

    general_authenticate_1[3] = seos_reader->params.key_no;
    bit_buffer_append_bytes(tx_buffer, general_authenticate_1, sizeof(general_authenticate_1));
    seos_log_bitbuffer(TAG, "NFC transmit", tx_buffer);

    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    seos_log_bitbuffer(TAG, "NFC response", rx_buffer);
    if(memcmp(
           bit_buffer_get_data(rx_buffer) + bit_buffer_get_size_bytes(rx_buffer) - sizeof(success),
           success,
           sizeof(success)) != 0) {
        FURI_LOG_W(TAG, "Non-success response");
        return NfcCommandStop;
    }

    // 7c0a8108018cde7d6049edb09000

    uint8_t expected_header[] = {0x7c, 0x0a, 0x81, 0x08};
    const uint8_t* rx_data = bit_buffer_get_data(rx_buffer);
    if(memcmp(rx_data, expected_header, sizeof(expected_header)) != 0) {
        FURI_LOG_W(TAG, "Invalid response");
        return NfcCommandStop;
    }

    memcpy(seos_reader->params.rndICC, rx_data + 4, 8);

    return ret;
}

NfcCommand seos_reader_general_authenticate_2(SeosReader* seos_reader) {
    Iso14443_4aPoller* iso14443_4a_poller = seos_reader->iso14443_4a_poller;
    BitBuffer* tx_buffer = seos_reader->tx_buffer;
    BitBuffer* rx_buffer = seos_reader->rx_buffer;
    FURI_LOG_I(TAG, "General Authenticate 2 with key no %d", seos_reader->params.key_no);

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aError error;

    uint8_t cryptogram[32 + 8];
    memset(cryptogram, 0, sizeof(cryptogram));
    seos_reader_generate_cryptogram(seos_reader->credential, &seos_reader->params, cryptogram);

    uint8_t ga_header[] = {
        0x00, 0x87, 0x00, seos_reader->params.key_no, 0x2c, 0x7c, 0x2a, 0x82, 0x28};

    bit_buffer_append_bytes(tx_buffer, ga_header, sizeof(ga_header));
    bit_buffer_append_bytes(tx_buffer, cryptogram, sizeof(cryptogram));
    seos_log_bitbuffer(TAG, "NFC transmit", tx_buffer);

    error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);

    seos_log_bitbuffer(TAG, "NFC response", rx_buffer);
    if(memcmp(
           bit_buffer_get_data(rx_buffer) + bit_buffer_get_size_bytes(rx_buffer) - sizeof(success),
           success,
           sizeof(success)) != 0) {
        FURI_LOG_W(TAG, "Non-success response");
        return NfcCommandStop;
    }

    const uint8_t* rx_data = bit_buffer_get_data(rx_buffer);
    if(rx_data[0] != 0x7C || rx_data[2] != 0x82) {
        FURI_LOG_W(TAG, "Invalid rx_data");
        return NfcCommandStop;
    }

    if(rx_data[3] == 40) {
        if(!seos_reader_verify_cryptogram(&seos_reader->params, rx_data + 4)) {
            FURI_LOG_W(TAG, "Card cryptogram failed verification");
            return NfcCommandStop;
        }
        FURI_LOG_I(TAG, "Authenticated successfully with key no %d", seos_reader->params.key_no);
    } else {
        FURI_LOG_W(TAG, "Unhandled card cryptogram size %d", rx_data[3]);
        ret = NfcCommandStop;
    }

    seos_reader->secure_messaging = secure_messaging_alloc(&seos_reader->params);

    return ret;
}

NfcCommand seos_state_machine(Seos* seos, Iso14443_4aPoller* iso14443_4a_poller) {
    furi_assert(seos);
    NfcCommand ret = NfcCommandContinue;

    SeosReader* seos_reader = seos_reader_alloc(seos->credential, iso14443_4a_poller);
    seos->seos_reader = seos_reader;

    do {
        ret = seos_reader_select_aid(seos_reader);
        if(ret == NfcCommandStop) break;

        ret = seos_reader_select_adf(seos_reader);
        if(ret == NfcCommandStop) break;

        if(!seos_reader_select_adf_response(
               seos_reader->rx_buffer, 0, seos_reader->credential, &seos_reader->params)) {
            ret = NfcCommandStop;
            break;
        }

        if(seos_reader->credential->write) {
            // Use Write keyslot
            seos_reader->params.key_no = 2;
        } else {
            // Use Read keyslot
            seos_reader->params.key_no = 1;
        }

        ret = seos_reader_general_authenticate_1(seos_reader);
        if(ret == NfcCommandStop) break;

        ret = seos_reader_general_authenticate_2(seos_reader);
        if(ret == NfcCommandStop) break;

        if(seos_reader->credential->write) {
            FURI_LOG_D(TAG, "Write SIO");
            if(seos_reader_write_sio(seos_reader)) {
                view_dispatcher_send_custom_event(
                    seos->view_dispatcher, SeosCustomEventPollerSuccess);
            } else {
                view_dispatcher_send_custom_event(
                    seos->view_dispatcher, SeosCustomEventPollerError);
            }
        } else {
            FURI_LOG_D(TAG, "Request SIO");
            if(seos_reader_request_sio(seos_reader)) {
                SeosCredential* credential = seos_reader->credential;
                AuthParameters* params = &seos_reader->params;

                memcpy(credential->priv_key, params->priv_key, sizeof(credential->priv_key));
                memcpy(credential->auth_key, params->auth_key, sizeof(credential->auth_key));
                credential->adf_oid_len = SEOS_ADF_OID_LEN;
                memcpy(credential->adf_oid, SEOS_ADF_OID, sizeof(credential->adf_oid));

                view_dispatcher_send_custom_event(
                    seos->view_dispatcher, SeosCustomEventPollerSuccess);
            } else {
                view_dispatcher_send_custom_event(
                    seos->view_dispatcher, SeosCustomEventPollerError);
            }
        }

    } while(false);

    // An error occurred
    if(ret == NfcCommandStop) {
        view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerError);
    }
    seos_reader_free(seos_reader);

    return NfcCommandStop;
}

NfcCommand seos_worker_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    NfcCommand ret = NfcCommandContinue;

    Seos* seos = context;

    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aPoller* iso14443_4a_poller = event.instance;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        // view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventPollerDetect);

        nfc_device_set_data(
            seos->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(seos->poller));

        ret = seos_state_machine(seos, iso14443_4a_poller);

        // furi_thread_set_current_priority(FuriThreadPriorityLowest);
    } else if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeError) {
        Iso14443_4aPollerEventData* data = iso14443_4a_event->data;
        Iso14443_4aError error = data->error;
        FURI_LOG_W(TAG, "Iso14443_4aError %i", error);
        // I was hoping to catch MFC here, but it seems to be treated the same (None) as no card being present.
        switch(error) {
        case Iso14443_4aErrorNone:
            break;
        case Iso14443_4aErrorNotPresent:
            break;
        case Iso14443_4aErrorProtocol:
            ret = NfcCommandStop;
            break;
        case Iso14443_4aErrorTimeout:
            break;
        default:
            break;
        }
    }

    return ret;
}
