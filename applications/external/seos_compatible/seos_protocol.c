#include "seos_protocol.h"

#include <string.h>

#include "keys.h"
#include "cmac.h"
#include "seos_tlv.h"

#define TAG "SeosProtocol"

const uint8_t SEOS_SW_SUCCESS[2] = {0x90, 0x00};
const uint8_t SEOS_SW_FILE_NOT_FOUND[2] = {0x6A, 0x82};

static uint8_t general_authenticate_1_response_header[] = {0x7c, 0x0a, 0x81, 0x08};

/* CLA, INS, P1 and the key number of the second authenticate command. */
#define GENERAL_AUTHENTICATE_2_HEADER_LEN 4

/* CLA, INS and P1 of the authenticate command. P2 is the keyset. */
static const uint8_t general_authenticate_header[] = {0x00, 0x87, 0x00};

/* The body of the first step: a request for the card's challenge. */
static const uint8_t general_authenticate_1_body[] = {0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};

bool seos_is_general_authenticate_1(const uint8_t* apdu, size_t apdu_len) {
    /* The key number sits between the header and the body, so the command is
     * one byte longer than the two of them. */
    if(apdu_len < SEOS_GENERAL_AUTHENTICATE_1_LEN) {
        return false;
    }
    if(memcmp(apdu, general_authenticate_header, sizeof(general_authenticate_header)) != 0) {
        return false;
    }
    return memcmp(
               apdu + sizeof(general_authenticate_header) + 1,
               general_authenticate_1_body,
               sizeof(general_authenticate_1_body)) == 0;
}

bool seos_is_general_authenticate_2(const uint8_t* apdu, size_t apdu_len) {
    if(apdu_len < GENERAL_AUTHENTICATE_2_HEADER_LEN) return false;
    if(memcmp(apdu, general_authenticate_header, sizeof(general_authenticate_header)) != 0) {
        return false;
    }
    /* The first step is told apart by its body, which this is not. */
    return !seos_is_general_authenticate_1(apdu, apdu_len);
}

void seos_build_general_authenticate_1(
    uint8_t key_no,
    uint8_t out[SEOS_GENERAL_AUTHENTICATE_1_LEN]) {
    memcpy(out, general_authenticate_header, sizeof(general_authenticate_header));
    out[sizeof(general_authenticate_header)] = key_no;
    memcpy(
        out + sizeof(general_authenticate_header) + 1,
        general_authenticate_1_body,
        sizeof(general_authenticate_1_body));
}

/* Appends `len` bytes that carry no information. */
static void append_random(BitBuffer* tx_buffer, size_t len) {
    uint8_t chunk[48];
    while(len > 0) {
        size_t take = len < sizeof(chunk) ? len : sizeof(chunk);
        seos_worker_random_nonce(chunk, take);
        bit_buffer_append_bytes(tx_buffer, chunk, take);
        len -= take;
    }
}

void seos_emulator_shill_select_adf(BitBuffer* tx_buffer) {
    /* The same shape a real answer has: the algorithm pair, a cryptogram, and
     * a checksum over it. */
    uint8_t header[] = {0xcd, 0x02, AES_128_CBC, SHA256, 0x85, 0x40};
    bit_buffer_append_bytes(tx_buffer, header, sizeof(header));
    append_random(tx_buffer, 0x40);

    uint8_t checksum_prefix[] = {0x8e, SEOS_WORKER_CMAC_SIZE};
    bit_buffer_append_bytes(tx_buffer, checksum_prefix, sizeof(checksum_prefix));
    append_random(tx_buffer, SEOS_WORKER_CMAC_SIZE);

    bit_buffer_append_bytes(tx_buffer, SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
}

void seos_emulator_shill_authenticate(BitBuffer* tx_buffer) {
    uint8_t header[] = {0x7c, 0x2a, 0x82, 0x28};
    bit_buffer_append_bytes(tx_buffer, header, sizeof(header));
    append_random(tx_buffer, 0x28);
    bit_buffer_append_bytes(tx_buffer, SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
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

    /* Header, the tag and length bytes ahead of the cryptogram, then the
     * cryptogram and its checksum. */
    const size_t cryptogram_offset = GENERAL_AUTHENTICATE_2_HEADER_LEN + 5;
    const size_t encrypted_len = 32;
    if(buffer_len < cryptogram_offset + encrypted_len + SEOS_WORKER_CMAC_SIZE) {
        FURI_LOG_W(TAG, "Authenticate frame too short (%d)", buffer_len);
        return false;
    }

    uint8_t* rx_data = (uint8_t*)buffer;
    uint8_t* cryptogram = rx_data + cryptogram_offset;
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
    if(!seos_cipher_cmac(
           params->cipher,
           params->auth_key,
           sizeof(params->auth_key),
           cryptogram,
           encrypted_len,
           cmac)) {
        return false;
    }

    if(memcmp(cmac, mac, SEOS_WORKER_CMAC_SIZE) != 0) {
        FURI_LOG_W(TAG, "Incorrect cryptogram mac %02x... vs %02x...", cmac[0], mac[0]);
        return false;
    }

    uint8_t clear[32];
    if(!seos_cipher_decrypt(params->cipher, params->priv_key, encrypted_len, cryptogram, clear)) {
        return false;
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
    if(!seos_cipher_encrypt(params->cipher, params->priv_key, sizeof(clear), clear, encrypted) ||
       !seos_cipher_cmac(
           params->cipher,
           params->auth_key,
           sizeof(params->auth_key),
           encrypted,
           sizeof(encrypted),
           cmac)) {
        return false;
    }

    bit_buffer_append_bytes(tx_buffer, response_header, sizeof(response_header));
    bit_buffer_append_bytes(tx_buffer, encrypted, sizeof(encrypted));
    bit_buffer_append_bytes(tx_buffer, cmac, SEOS_WORKER_CMAC_SIZE);

    return true;
}

void seos_emulator_des_adf_payload(SeosCredential* credential, uint8_t* buffer) {
    /* A fresh random half-block and its checksum, sent as the first block. */
    uint8_t rnd[4];
    seos_worker_random_nonce(rnd, sizeof(rnd));
    uint8_t cmac[8] = {0};
    /// cmac
    des_cmac(SEOS_ADF1_PRIV_MAC, sizeof(SEOS_ADF1_PRIV_MAC), rnd, sizeof(rnd), cmac);
    uint8_t iv[8];
    memcpy(iv + 0, rnd, sizeof(rnd));
    memcpy(iv + sizeof(rnd), cmac, sizeof(iv) - sizeof(rnd));

    // Copy IV to buffer because mbedtls_des3_crypt_cbc mutates it
    memcpy(buffer + 0, iv, sizeof(iv));

    uint8_t clear[0x30];
    seos_worker_random_nonce(clear, sizeof(clear));
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
    /* The initialisation vector is built from a fresh random half-block and
     * its checksum, and sent as the first block. Without the randomness every
     * select answer is identical, which is exactly what a card watching for
     * this would look for. */
    uint8_t rnd[8];
    seos_worker_random_nonce(rnd, sizeof(rnd));
    uint8_t cmac[16] = {0};
    /// cmac
    aes_cmac(SEOS_ADF1_PRIV_MAC, sizeof(SEOS_ADF1_PRIV_MAC), rnd, sizeof(rnd), cmac);
    uint8_t iv[16];
    memcpy(iv + 0, rnd, sizeof(rnd));
    memcpy(iv + sizeof(rnd), cmac, sizeof(iv) - sizeof(rnd));

    // Copy IV to buffer because mbedtls_aes_crypt_cbc mutates it
    memcpy(buffer + 0, iv, sizeof(iv));

    uint8_t clear[0x30];
    seos_worker_random_nonce(clear, sizeof(clear));
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

/* How much of a saved select answer to send, or zero if it is not one.
 *
 * The length comes out of the saved bytes themselves, so it has to be checked
 * against the field holding them: a credential written by something else, or
 * damaged in storage, would otherwise have us read past the end of it and put
 * whatever followed on the air. */
static size_t saved_adf_response_length(const SeosCredential* credential) {
    const uint8_t* saved = credential->adf_response;
    size_t capacity = sizeof(credential->adf_response);

    /* The algorithm object, then the cryptogram. */
    if(capacity < 6 || saved[0] != 0xCD || saved[1] != 0x02 || saved[4] != 0x85) {
        return 0;
    }

    size_t cryptogram_len = saved[5];
    if(cryptogram_len >= 0x80) {
        /* A long form length, which no answer that fits here would need. */
        FURI_LOG_W(TAG, "Saved ADF response length not understood");
        return 0;
    }

    /* Algorithm object, cryptogram header and body, then the checksum. */
    size_t total = 4 + 2 + cryptogram_len + 2 + SEOS_WORKER_CMAC_SIZE;
    if(total > capacity) {
        FURI_LOG_W(TAG, "Saved ADF response claims %d bytes", total);
        return 0;
    }

    if(saved[6 + cryptogram_len] != 0x8E || saved[7 + cryptogram_len] != SEOS_WORKER_CMAC_SIZE) {
        FURI_LOG_W(TAG, "Saved ADF response has no checksum where one belongs");
        return 0;
    }

    return total;
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

            size_t saved_len = saved_adf_response_length(credential);
            if(saved_len > 0) {
                FURI_LOG_I(TAG, "Using saved ADF Response");
                bit_buffer_append_bytes(tx_buffer, credential->adf_response, saved_len);

                /* The saved answer names the cipher and digest the session
                 * runs under, which need not be what we started assuming. */
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
    memset(cmac, 0, sizeof(cmac));
    if(!seos_cipher_encrypt(params->cipher, params->priv_key, sizeof(clear), clear, cryptogram) ||
       !seos_cipher_cmac(
           params->cipher, params->auth_key, sizeof(params->auth_key), cryptogram, index, cmac)) {
        return;
    }
    memcpy(cryptogram + sizeof(clear), cmac, SEOS_WORKER_CMAC_SIZE);
}

bool seos_reader_verify_cryptogram(AuthParameters* params, const uint8_t* cryptogram) {
    // cryptogram is 40 bytes: 32 byte encrypted + 8 byte cmac
    size_t encrypted_len = 32;
    uint8_t* mac = (uint8_t*)cryptogram + encrypted_len;
    uint8_t cmac[16];
    if(!seos_cipher_cmac(
           params->cipher,
           params->auth_key,
           sizeof(params->auth_key),
           (uint8_t*)cryptogram,
           encrypted_len,
           cmac)) {
        return false;
    }

    if(memcmp(cmac, mac, SEOS_WORKER_CMAC_SIZE) != 0) {
        FURI_LOG_W(TAG, "Incorrect cryptogram mac %02x... vs %02x...", cmac[0], mac[0]);
        return false;
    }

    uint8_t clear[32];
    memset(clear, 0, sizeof(clear));
    if(!seos_cipher_decrypt(params->cipher, params->priv_key, encrypted_len, cryptogram, clear)) {
        return false;
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
/* Data objects the authenticate answers are built from. */
#define DO_DYNAMIC_AUTH   0x7c
#define DO_CARD_CHALLENGE 0x81
#define DO_CARD_RESPONSE  0x82

/* Objects the answer to a select carries, and the two inside its cryptogram. */
#define DO_ADF_ALGORITHMS  0xcd
#define DO_ADF_CRYPTOGRAM  0x85
#define DO_ADF_OID         0x06
#define DO_ADF_DIVERSIFIER 0xcf

/* CLA, INS, P1 and P2 of a select by name, and of a select by application
 * identifier. */
static const uint8_t select_aid_command_header[] = {0x00, 0xa4, 0x04, 0x00};
static const uint8_t select_adf_command_header[] = {0x80, 0xa5, 0x04, 0x00};

/* Both selects state the length of what follows in the byte after the header.
 * Shared so the bound is written once. */
static bool parse_select_body(
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t* header,
    size_t header_len,
    const uint8_t** body,
    size_t* body_len) {
    if(apdu_len <= header_len) return false;
    if(memcmp(apdu, header, header_len) != 0) return false;

    size_t stated = apdu[header_len];
    size_t body_offset = header_len + 1;

    if(stated == 0) return false;
    /* Compared rather than added: body_offset + stated could wrap. */
    if(stated > apdu_len - body_offset) return false;

    *body = apdu + body_offset;
    *body_len = stated;
    return true;
}

bool seos_parse_select_aid(
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t** aid,
    size_t* aid_len) {
    return parse_select_body(
        apdu, apdu_len, select_aid_command_header, sizeof(select_aid_command_header), aid, aid_len);
}

bool seos_parse_select_adf(
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t** oid_list,
    size_t* oid_list_len) {
    return parse_select_body(
        apdu,
        apdu_len,
        select_adf_command_header,
        sizeof(select_adf_command_header),
        oid_list,
        oid_list_len);
}

bool seos_response_status(const uint8_t* data, size_t len, uint16_t* status_word) {
    if(len < sizeof(uint16_t)) return false;

    *status_word = (uint16_t)((data[len - 2] << 8) | data[len - 1]);
    return true;
}

/* Reads the object a dynamic authentication wrapper carries. */
static bool read_authenticate_object(
    const uint8_t* data,
    size_t len,
    uint16_t expected_tag,
    const uint8_t** value,
    size_t* value_len) {
    SeosTlvObject wrapper;
    if(!seos_tlv_read_at(data, len, 0, &wrapper) || wrapper.tag != DO_DYNAMIC_AUTH) return false;

    SeosTlvCursor inner;
    SeosTlvObject object;
    seos_tlv_cursor_init(&inner, wrapper.value, wrapper.value_len);
    if(!seos_tlv_read(&inner, &object) || object.tag != expected_tag) return false;

    *value = object.value;
    *value_len = object.value_len;
    return true;
}

bool seos_parse_ga1_response(const uint8_t* data, size_t len, uint8_t* rnd_icc, size_t rnd_icc_len) {
    const uint8_t* value = NULL;
    size_t value_len = 0;
    if(!read_authenticate_object(data, len, DO_CARD_CHALLENGE, &value, &value_len)) return false;

    /* A challenge of another length is not the one the session is built on. */
    if(value_len != rnd_icc_len) return false;

    memcpy(rnd_icc, value, value_len);
    return true;
}

bool seos_parse_ga2_response(
    const uint8_t* data,
    size_t len,
    const uint8_t** cryptogram,
    size_t* cryptogram_len) {
    return read_authenticate_object(data, len, DO_CARD_RESPONSE, cryptogram, cryptogram_len);
}

bool seos_parse_sio_response(
    const uint8_t* data,
    size_t len,
    uint8_t* sio,
    size_t sio_cap,
    size_t* sio_len) {
    SeosTlvObject object;
    if(!seos_tlv_read_at(data, len, 0, &object) || object.tag != SEOS_SIO_FILE_TAG) return false;
    if(object.value_len > sio_cap) return false;

    memcpy(sio, object.value, object.value_len);
    *sio_len = object.value_len;
    return true;
}

bool seos_reader_write_accepted(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    if(!secure_messaging_unwrap_rapdu(secure_messaging, rx_buffer)) {
        FURI_LOG_W(TAG, "Could not unwrap the write answer");
        return false;
    }

    if(secure_messaging->last_response_sw != SEOS_SW_SUCCESS_VALUE) {
        FURI_LOG_W(TAG, "Write answered %04x", secure_messaging->last_response_sw);
        return false;
    }

    return true;
}

bool seos_reader_select_adf_response(
    BitBuffer* rx_buffer,
    size_t offset,
    SeosCredential* credential,
    AuthParameters* params) {
    /* The answer may sit behind a byte of transport framing, so `offset` says
     * where it starts. Everything below counts from there: a buffer that does
     * not reach past the offset has nothing to read, and a length measured
     * from it would run backwards past zero. */
    size_t rx_len = bit_buffer_get_size_bytes(rx_buffer);
    if(rx_len <= offset) {
        FURI_LOG_W(TAG, "Invalid response length");
        return false;
    }
    const uint8_t* rx_data = bit_buffer_get_data(rx_buffer) + offset;
    size_t body_len = rx_len - offset;

    /* The status word closes the answer and is not one of the objects. */
    if(body_len < sizeof(SEOS_SW_SUCCESS)) {
        FURI_LOG_W(TAG, "Invalid response length");
        return false;
    }
    body_len -= sizeof(SEOS_SW_SUCCESS);

    SeosTlvCursor cursor;
    seos_tlv_cursor_init(&cursor, rx_data, body_len);

    /* Which cipher and hash the card picked. */
    SeosTlvObject algorithms;
    if(!seos_tlv_read(&cursor, &algorithms) || algorithms.tag != DO_ADF_ALGORITHMS ||
       algorithms.value_len != 2) {
        FURI_LOG_W(TAG, "Invalid response");
        return false;
    }
    params->cipher = algorithms.value[0];
    params->hash = algorithms.value[1];

    SeosTlvObject cryptogram;
    if(!seos_tlv_read(&cursor, &cryptogram) || cryptogram.tag != DO_ADF_CRYPTOGRAM) {
        FURI_LOG_W(TAG, "No cryptogram in the select answer");
        return false;
    }

    /* Kept whole so the answer can be replayed when this credential is
     * emulated. */
    memset(credential->adf_response, 0, sizeof(credential->adf_response));
    size_t response_length = body_len;
    if(response_length > sizeof(credential->adf_response)) {
        FURI_LOG_W(
            TAG,
            "adf_response too large %zu > %zu",
            response_length,
            sizeof(credential->adf_response));
        response_length = sizeof(credential->adf_response);
    }
    memcpy(credential->adf_response, rx_data, response_length);

    /* The cryptogram opens with the initialisation vector, one block long,
     * and the rest is what was encrypted under it. */
    uint8_t clear[0x40];
    memset(clear, 0, sizeof(clear));

    size_t block_size = seos_cipher_block_size(params->cipher);
    if(block_size == 0) {
        FURI_LOG_W(TAG, "Unknown cipher (%d)", params->cipher);
        return false;
    }
    if(cryptogram.value_len <= block_size) {
        FURI_LOG_W(TAG, "Cryptogram carries no more than its vector");
        return false;
    }

    size_t enc_len = cryptogram.value_len - block_size;
    if(enc_len > sizeof(clear) || (enc_len % block_size) != 0) {
        FURI_LOG_W(TAG, "Cryptogram of %zu will not decrypt", enc_len);
        return false;
    }

    /* Copied because the cipher advances the vector as it works. */
    uint8_t iv[16];
    memcpy(iv, cryptogram.value, block_size);
    const uint8_t* enc = cryptogram.value + block_size;

    if(params->cipher == AES_128_CBC) {
        mbedtls_aes_context ctx;
        mbedtls_aes_init(&ctx);
        mbedtls_aes_setkey_dec(&ctx, SEOS_ADF1_PRIV_ENC, sizeof(SEOS_ADF1_PRIV_ENC) * 8);
        mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, enc_len, iv, enc, clear);
        mbedtls_aes_free(&ctx);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_dec(&ctx, SEOS_ADF1_PRIV_ENC);
        mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, enc_len, iv, enc, clear);
        mbedtls_des3_free(&ctx);
    } else {
        FURI_LOG_W(TAG, "Unhandled cipher (%d)", params->cipher);
        return false;
    }

    /* What comes back is an application identifier and the diversifier the
     * card's keys were derived with. Read only as far as was decrypted: past
     * that is padding, and past that is nothing. */
    SeosTlvCursor clear_cursor;
    seos_tlv_cursor_init(&clear_cursor, clear, enc_len);

    SeosTlvObject oid;
    if(!seos_tlv_read(&clear_cursor, &oid) || oid.tag != DO_ADF_OID) {
        FURI_LOG_W(TAG, "No application identifier in the cryptogram");
        return false;
    }

    SeosTlvObject diversifier;
    if(!seos_tlv_read(&clear_cursor, &diversifier) || diversifier.tag != DO_ADF_DIVERSIFIER) {
        FURI_LOG_W(TAG, "No diversifier after the application identifier");
        return false;
    }
    if(diversifier.value_len > sizeof(credential->diversifier)) {
        FURI_LOG_W(TAG, "diversifier too large");
        return false;
    }

    credential->diversifier_len = diversifier.value_len;
    memcpy(credential->diversifier, diversifier.value, diversifier.value_len);

    return true;
}
