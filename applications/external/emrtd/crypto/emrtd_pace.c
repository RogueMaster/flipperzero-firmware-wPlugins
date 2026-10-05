/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_pace.h"

#include <stdlib.h>
#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha1.h>

#include "../protocol/emrtd_apdu.h"
#include "../protocol/emrtd_mrz.h"
#include "../protocol/emrtd_tlv.h"
#include "../transport/emrtd_transceiver.h"
#include "emrtd_ec.h"
#include "emrtd_kdf.h"
#include "emrtd_mac.h"
#include "emrtd_rng.h"

#include "../emrtd_wipe.h"

/* The dynamic authentication data objects of ICAO 9303-11 table 4. */
#define EMRTD_PACE_DO_DYNAMIC   0x7C /**< The wrapper every step travels in. */
#define EMRTD_PACE_DO_NONCE     0x80 /**< Response: the encrypted nonce. */
#define EMRTD_PACE_DO_MAP_IFD   0x81 /**< Command: the terminal's mapping key. */
#define EMRTD_PACE_DO_MAP_IC    0x82 /**< Response: the chip's mapping key. */
#define EMRTD_PACE_DO_KA_IFD    0x83 /**< Command: the terminal's agreement key. */
#define EMRTD_PACE_DO_KA_IC     0x84 /**< Response: the chip's agreement key. */
#define EMRTD_PACE_DO_TOKEN_IFD 0x85 /**< Command: the terminal's token. */
#define EMRTD_PACE_DO_TOKEN_IC  0x86 /**< Response: the chip's token. */

/* The control reference template of MSE:Set AT (9303-11, 4.4.4.1). */
#define EMRTD_PACE_DO_OID      0x80
#define EMRTD_PACE_DO_PASSWORD 0x83

/** Password references of ICAO 9303-11 table 13. */
#define EMRTD_PACE_PASSWORD_MRZ 0x01
#define EMRTD_PACE_PASSWORD_CAN 0x02

/** Both authentication tokens are eight bytes (9303-11, 4.4.3.4). */
#define EMRTD_PACE_TOKEN_SIZE 8

/** The object identifier of a PACE protocol, as carried in PACEInfo. */
#define EMRTD_PACE_OID_TAG       0x06
/** Tag 86 holds a public point inside the token input. */
#define EMRTD_PACE_POINT_TAG     0x86
/** The token input is wrapped in the two byte tag 7F49. */
#define EMRTD_PACE_TOKEN_WRAPPER 0x7F49

/** Room for 7F49 { 06 oid 86 point } with the longest point and identifier. */
#define EMRTD_PACE_TOKEN_INPUT_MAX (8 + EMRTD_OID_MAX + EMRTD_EC_POINT_MAX)

/** Room for 7C { tag point }. */
#define EMRTD_PACE_DYNAMIC_MAX (6 + EMRTD_EC_POINT_MAX)

/**
 * The buffers one run of the protocol needs.
 *
 * They are gathered into one object so that the cost to the NFC thread's eight
 * kilobyte stack is a single number that can be read off this declaration,
 * rather than something scattered over the function below.
 */
typedef struct {
    EmrtdTransceiver* transceiver;
    uint8_t command[EMRTD_APDU_MAX_SIZE];
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    uint8_t dynamic[EMRTD_PACE_DYNAMIC_MAX];
    uint8_t token_input[EMRTD_PACE_TOKEN_INPUT_MAX];
} EmrtdPaceIo;

/** Compare without letting the time taken reveal where two values first differ. */
static bool emrtd_pace_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for(size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/** True when the Card Access Number is the password to use. */
static bool emrtd_pace_uses_can(const EmrtdCredentials* credentials) {
    return credentials->has_can && credentials->can[0] != '\0';
}

/**
 * Derive Kpi for a given cipher, ICAO 9303-11 section 9.7.3 and table 11.
 *
 * The password itself is the CAN as printed, or the SHA-1 digest of the MRZ
 * information. The key derivation that follows belongs to the cipher the
 * PACEInfo names, so an AES-192 or AES-256 protocol gets a longer Kpi off
 * SHA-256 rather than a truncated one off SHA-1.
 */
static EmrtdError emrtd_pace_password_key_for_cipher(
    const EmrtdCredentials* credentials,
    EmrtdCipher cipher,
    uint8_t* out,
    size_t out_size) {
    if(credentials == NULL || out == NULL) {
        return EmrtdErrorInternal;
    }
    if(out_size < emrtd_cipher_key_size(cipher) || emrtd_cipher_key_size(cipher) == 0) {
        return EmrtdErrorBufferTooSmall;
    }

    EmrtdError error = EmrtdErrorNone;
    uint8_t password[20];
    size_t password_len = 0;

    if(emrtd_pace_uses_can(credentials)) {
        password_len = strlen(credentials->can);
        if(password_len == 0 || password_len > sizeof(password)) {
            return EmrtdErrorInvalidInput;
        }
        memcpy(password, credentials->can, password_len);
    } else {
        char information[EMRTD_MRZ_INFO_MAX];
        error = emrtd_mrz_information(credentials, information, sizeof(information));
        if(error == EmrtdErrorNone) {
            if(mbedtls_sha1((const unsigned char*)information, strlen(information), password) !=
               0) {
                error = EmrtdErrorInternal;
            }
            password_len = 20;
        }
        emrtd_secure_wipe(information, sizeof(information));
    }

    if(error == EmrtdErrorNone &&
       !emrtd_kdf(cipher, password, password_len, EMRTD_KDF_COUNTER_PACE, out)) {
        error = EmrtdErrorInternal;
    }

    emrtd_secure_wipe(password, sizeof(password));
    return error;
}

EmrtdError emrtd_pace_password_key(const EmrtdCredentials* credentials, uint8_t out[16]) {
    /*
     * The published contract is the sixteen byte key, which is what every
     * AES-128 and 3DES protocol uses and what the appendix G vector pins.
     */
    return emrtd_pace_password_key_for_cipher(credentials, EmrtdCipherAes128, out, 16);
}

EmrtdError emrtd_pace_token_input(
    const uint8_t* oid,
    size_t oid_len,
    const uint8_t* point,
    size_t point_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(oid == NULL || point == NULL || out == NULL || out_len == NULL) {
        return EmrtdErrorInternal;
    }
    if(oid_len == 0 || oid_len > 0x7F || point_len == 0) {
        return EmrtdErrorInvalidInput;
    }

    const size_t inner_len =
        2 + oid_len + 1 + emrtd_tlv_encoded_length_size(point_len) + point_len;
    const size_t total = 2 + emrtd_tlv_encoded_length_size(inner_len) + inner_len;
    if(total > out_size) {
        return EmrtdErrorBufferTooSmall;
    }

    size_t offset = 0;
    out[offset++] = (uint8_t)(EMRTD_PACE_TOKEN_WRAPPER >> 8);
    out[offset++] = (uint8_t)(EMRTD_PACE_TOKEN_WRAPPER & 0xFF);
    offset += emrtd_tlv_encode_length(inner_len, out + offset);

    out[offset++] = EMRTD_PACE_OID_TAG;
    out[offset++] = (uint8_t)oid_len;
    memcpy(out + offset, oid, oid_len);
    offset += oid_len;

    out[offset++] = EMRTD_PACE_POINT_TAG;
    offset += emrtd_tlv_encode_length(point_len, out + offset);
    memcpy(out + offset, point, point_len);
    offset += point_len;

    *out_len = offset;
    return EmrtdErrorNone;
}

/**
 * The checksum a PACE token is made of, ICAO 9303-11 section 4.4.3.4.
 *
 * Which algorithm that is follows the cipher: AES-CMAC, which pads the input
 * itself, or the Retail MAC over data padded here with ISO/IEC 9797-1 method 2.
 */
static EmrtdError emrtd_pace_token(
    EmrtdCipher cipher,
    const uint8_t* ks_mac,
    uint8_t* input,
    size_t input_len,
    size_t input_size,
    uint8_t token[EMRTD_PACE_TOKEN_SIZE]) {
    if(cipher == EmrtdCipherTdes) {
        if(emrtd_padded_len(input_len, 8) > input_size) {
            return EmrtdErrorBufferTooSmall;
        }
        const size_t padded = emrtd_pad_iso9797_m2(input, input_len, 8);
        return emrtd_retail_mac(ks_mac, input, padded, token) ? EmrtdErrorNone :
                                                                EmrtdErrorInternal;
    }
    return emrtd_aes_cmac(
               ks_mac,
               emrtd_cipher_key_size(cipher),
               input,
               input_len,
               token,
               EMRTD_PACE_TOKEN_SIZE) ?
               EmrtdErrorNone :
               EmrtdErrorInternal;
}

/** Decrypt the nonce with Kpi, CBC and a zero IV (ICAO 9303-11, 4.4.3.1). */
static EmrtdError emrtd_pace_decrypt_nonce(
    EmrtdCipher cipher,
    const uint8_t* kpi,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t iv[EMRTD_BLOCK_MAX_SIZE];
    memset(iv, 0, sizeof(iv));
    EmrtdError error = EmrtdErrorNone;

    if(cipher == EmrtdCipherTdes) {
        mbedtls_des3_context des3;
        mbedtls_des3_init(&des3);
        if(mbedtls_des3_set2key_dec(&des3, kpi) != 0 ||
           mbedtls_des3_crypt_cbc(&des3, MBEDTLS_DES_DECRYPT, len, iv, in, out) != 0) {
            error = EmrtdErrorInternal;
        }
        mbedtls_des3_free(&des3);
    } else {
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        const unsigned int key_bits = (unsigned int)(emrtd_cipher_key_size(cipher) * 8);
        if(mbedtls_aes_setkey_dec(&aes, kpi, key_bits) != 0 ||
           mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, in, out) != 0) {
            error = EmrtdErrorInternal;
        }
        mbedtls_aes_free(&aes);
    }

    memset(iv, 0, sizeof(iv));
    return error;
}

/** Turn a status word into the most useful error this step can report. */
static EmrtdError emrtd_pace_status_error(uint16_t sw, bool token_step) {
    /*
     * 63CX is the chip counting down the attempts it will still allow, which
     * it sends when the terminal's token did not verify - that is, when the
     * MRZ input or the CAN is not this document's.
     */
    if(token_step && (sw >> 8) == 0x63) {
        return EmrtdErrorWrongKey;
    }
    const EmrtdError error = emrtd_error_from_sw(sw);
    return error == EmrtdErrorNone ? EmrtdErrorProtocol : error;
}

/** Send one command APDU in the clear and split the answer. */
static EmrtdError
    emrtd_pace_send(EmrtdPaceIo* io, const EmrtdCommandApdu* command, EmrtdResponseApdu* out) {
    size_t command_len = 0;
    EmrtdError error = emrtd_apdu_encode(command, io->command, sizeof(io->command), &command_len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    size_t response_len = 0;
    error = emrtd_transceiver_exchange(
        io->transceiver,
        io->command,
        command_len,
        io->response,
        sizeof(io->response),
        &response_len);
    if(error != EmrtdErrorNone) {
        return error;
    }
    return emrtd_apdu_decode(io->response, response_len, out);
}

/**
 * One General Authenticate round.
 *
 * The command data is a DO 7C holding at most one object; the first round
 * sends it empty, which is what asks the chip for its nonce. @p inner_tag of
 * zero means exactly that. The answer is expected to be a DO 7C holding
 * @p expected_tag, and what comes back points into the response buffer, so it
 * stays valid only until the next round.
 */
static EmrtdError emrtd_pace_general_authenticate(
    EmrtdPaceIo* io,
    uint8_t inner_tag,
    const uint8_t* value,
    size_t value_len,
    bool chaining,
    uint8_t expected_tag,
    const uint8_t** out_value,
    size_t* out_value_len) {
    size_t inner_len = 0;
    if(inner_tag != 0) {
        inner_len = 1 + emrtd_tlv_encoded_length_size(value_len) + value_len;
    }
    const size_t total = 1 + emrtd_tlv_encoded_length_size(inner_len) + inner_len;
    if(total > sizeof(io->dynamic)) {
        return EmrtdErrorBufferTooSmall;
    }

    size_t offset = 0;
    io->dynamic[offset++] = EMRTD_PACE_DO_DYNAMIC;
    offset += emrtd_tlv_encode_length(inner_len, io->dynamic + offset);
    if(inner_tag != 0) {
        io->dynamic[offset++] = inner_tag;
        offset += emrtd_tlv_encode_length(value_len, io->dynamic + offset);
        memcpy(io->dynamic + offset, value, value_len);
        offset += value_len;
    }

    EmrtdCommandApdu command;
    emrtd_apdu_general_authenticate(&command, io->dynamic, offset, chaining);

    EmrtdResponseApdu response;
    EmrtdError error = emrtd_pace_send(io, &command, &response);
    if(error != EmrtdErrorNone) {
        return error;
    }
    if(!emrtd_apdu_is_success(&response)) {
        return emrtd_pace_status_error(response.sw, expected_tag == EMRTD_PACE_DO_TOKEN_IC);
    }

    EmrtdTlv wrapper;
    if(!emrtd_tlv_parse_first(response.data, response.data_len, &wrapper) ||
       wrapper.tag != EMRTD_PACE_DO_DYNAMIC) {
        return EmrtdErrorParse;
    }

    EmrtdTlv node;
    if(!emrtd_tlv_find(wrapper.value, wrapper.value_len, expected_tag, &node)) {
        return EmrtdErrorParse;
    }
    *out_value = node.value;
    *out_value_len = node.value_len;
    return EmrtdErrorNone;
}

/** MSE:Set AT, naming the protocol and which password opens it (9303-11, 4.4.4.1). */
static EmrtdError emrtd_pace_set_at(
    EmrtdPaceIo* io,
    const EmrtdPaceInfo* info,
    const EmrtdCredentials* credentials) {
    if(info->oid_len == 0 || info->oid_len > EMRTD_OID_MAX) {
        return EmrtdErrorInvalidInput;
    }

    size_t offset = 0;
    io->dynamic[offset++] = EMRTD_PACE_DO_OID;
    io->dynamic[offset++] = (uint8_t)info->oid_len;
    memcpy(io->dynamic + offset, info->oid, info->oid_len);
    offset += info->oid_len;
    io->dynamic[offset++] = EMRTD_PACE_DO_PASSWORD;
    io->dynamic[offset++] = 0x01;
    io->dynamic[offset++] = emrtd_pace_uses_can(credentials) ? EMRTD_PACE_PASSWORD_CAN :
                                                               EMRTD_PACE_PASSWORD_MRZ;

    EmrtdCommandApdu command;
    emrtd_apdu_mse_set_at(&command, io->dynamic, offset);

    EmrtdResponseApdu response;
    const EmrtdError error = emrtd_pace_send(io, &command, &response);
    if(error != EmrtdErrorNone) {
        return error;
    }
    return emrtd_apdu_is_success(&response) ? EmrtdErrorNone :
                                              emrtd_pace_status_error(response.sw, false);
}

EmrtdError emrtd_pace_run(
    EmrtdTransceiver* transceiver,
    const EmrtdPaceInfo* info,
    const EmrtdCredentials* credentials,
    EmrtdSm* out_session) {
    if(transceiver == NULL || info == NULL || credentials == NULL || out_session == NULL) {
        return EmrtdErrorInternal;
    }

    /*
     * Everything this build cannot do is refused by name before a single APDU
     * is sent, so that the screen can say which curve or which mapping the
     * document asked for instead of reporting a bare failure. The reasons are
     * worked out when EF.CardAccess is parsed; see docs/platform.md for why
     * the list is what it is.
     */
    if(info->agreement != EmrtdPaceAgreementEcdh) {
        return EmrtdErrorPaceUnsupportedDh;
    }
    if(info->mapping != EmrtdPaceMappingGeneric) {
        return EmrtdErrorPaceUnsupportedMapping;
    }
    if(info->curve == NULL || !emrtd_ec_curve_supported(info->curve)) {
        return EmrtdErrorPaceUnsupportedCurve;
    }
    if(!info->usable) {
        return info->reason != EmrtdErrorNone ? info->reason : EmrtdErrorUnsupported;
    }
    if(!emrtd_pace_uses_can(credentials) && !emrtd_credentials_valid_mrz(credentials)) {
        return EmrtdErrorInvalidInput;
    }

    const EmrtdCurve* const curve = info->curve;
    const size_t block = emrtd_cipher_block_size(info->cipher);

    /*
     * The buffers live on the heap. Run on the NFC stack's own thread, which
     * has eight kilobytes to itself, and with the elliptic curve arithmetic
     * below this frame wanting a good part of that, most of a kilobyte of
     * command and response buffers does not belong on the stack.
     */
    EmrtdPaceIo* io = NULL;

    uint8_t kpi[EMRTD_KEY_MAX_SIZE];
    uint8_t nonce[EMRTD_BLOCK_MAX_SIZE];
    uint8_t point[EMRTD_EC_POINT_MAX];
    uint8_t shared_x[EMRTD_EC_COORD_MAX];
    uint8_t ks_enc[EMRTD_KEY_MAX_SIZE];
    uint8_t ks_mac[EMRTD_KEY_MAX_SIZE];
    uint8_t ssc[EMRTD_BLOCK_MAX_SIZE];
    uint8_t token_ifd[EMRTD_PACE_TOKEN_SIZE];
    uint8_t token_expected[EMRTD_PACE_TOKEN_SIZE];
    size_t point_len = 0;
    size_t token_input_len = 0;

    mbedtls_ecp_group group;
    mbedtls_mpi s;
    mbedtls_mpi one;
    mbedtls_mpi mapping_key;
    mbedtls_mpi agreement_key;
    mbedtls_ecp_point mapping_ifd;
    mbedtls_ecp_point mapping_ic;
    mbedtls_ecp_point mapping_shared;
    mbedtls_ecp_point generator;
    mbedtls_ecp_point agreement_ifd;
    mbedtls_ecp_point agreement_ic;
    mbedtls_ecp_point agreement_shared;

    mbedtls_ecp_group_init(&group);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&one);
    mbedtls_mpi_init(&mapping_key);
    mbedtls_mpi_init(&agreement_key);
    mbedtls_ecp_point_init(&mapping_ifd);
    mbedtls_ecp_point_init(&mapping_ic);
    mbedtls_ecp_point_init(&mapping_shared);
    mbedtls_ecp_point_init(&generator);
    mbedtls_ecp_point_init(&agreement_ifd);
    mbedtls_ecp_point_init(&agreement_ic);
    mbedtls_ecp_point_init(&agreement_shared);

    emrtd_secure_wipe(kpi, sizeof(kpi));
    emrtd_secure_wipe(ks_enc, sizeof(ks_enc));
    emrtd_secure_wipe(ks_mac, sizeof(ks_mac));

    const uint8_t* peer = NULL;
    size_t peer_len = 0;

    EmrtdError error = EmrtdErrorNone;

    io = malloc(sizeof(EmrtdPaceIo));
    if(io == NULL) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }
    io->transceiver = transceiver;

    error = emrtd_pace_password_key_for_cipher(credentials, info->cipher, kpi, sizeof(kpi));
    if(error != EmrtdErrorNone) goto cleanup;

    if(emrtd_ec_group_load(&group, curve) != 0) {
        error = EmrtdErrorPaceUnsupportedCurve;
        goto cleanup;
    }

    /* Step 1: name the protocol and the password. */
    error = emrtd_pace_set_at(io, info, credentials);
    if(error != EmrtdErrorNone) goto cleanup;

    /*
     * Step 2: the encrypted nonce. The request carries an empty DO 7C; sending
     * anything else here is what makes a real chip answer 6A80.
     */
    error = emrtd_pace_general_authenticate(
        io, 0, NULL, 0, true, EMRTD_PACE_DO_NONCE, &peer, &peer_len);
    if(error != EmrtdErrorNone) goto cleanup;
    if(peer_len != block) {
        /* The nonce is exactly one cipher block (9303-11, 4.4.3.1). */
        error = EmrtdErrorProtocol;
        goto cleanup;
    }
    error = emrtd_pace_decrypt_nonce(info->cipher, kpi, peer, peer_len, nonce);
    if(error != EmrtdErrorNone) goto cleanup;

    /*
     * Step 3: the generic mapping. An ephemeral exchange produces H, and the
     * nonce maps the generator onto G_hat = s * G + H (9303-11, 4.4.3.3.2).
     */
    if(mbedtls_ecp_gen_privkey(&group, &mapping_key, emrtd_random_mbedtls, NULL) != 0 ||
       mbedtls_ecp_mul(&group, &mapping_ifd, &mapping_key, &group.G, emrtd_random_mbedtls, NULL) !=
           0 ||
       emrtd_ec_point_write(&group, &mapping_ifd, point, sizeof(point), &point_len) != 0) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }

    error = emrtd_pace_general_authenticate(
        io, EMRTD_PACE_DO_MAP_IFD, point, point_len, true, EMRTD_PACE_DO_MAP_IC, &peer, &peer_len);
    if(error != EmrtdErrorNone) goto cleanup;

    /* emrtd_ec_point_read() refuses a point off the curve before it is multiplied. */
    if(emrtd_ec_point_read(&group, &mapping_ic, peer, peer_len) != 0) {
        error = EmrtdErrorPaceFailed;
        goto cleanup;
    }
    if(mbedtls_mpi_read_binary(&s, nonce, block) != 0 || mbedtls_mpi_lset(&one, 1) != 0 ||
       mbedtls_ecp_mul(
           &group, &mapping_shared, &mapping_key, &mapping_ic, emrtd_random_mbedtls, NULL) != 0 ||
       mbedtls_ecp_muladd(&group, &generator, &s, &group.G, &one, &mapping_shared) != 0) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }
    if(mbedtls_ecp_is_zero(&generator) || mbedtls_ecp_check_pubkey(&group, &generator) != 0) {
        /* A degenerate mapped generator would agree on nothing. */
        error = EmrtdErrorPaceFailed;
        goto cleanup;
    }

    /* Step 4: the key agreement, over the mapped generator rather than G. */
    if(mbedtls_ecp_gen_privkey(&group, &agreement_key, emrtd_random_mbedtls, NULL) != 0 ||
       mbedtls_ecp_mul(
           &group, &agreement_ifd, &agreement_key, &generator, emrtd_random_mbedtls, NULL) != 0 ||
       emrtd_ec_point_write(&group, &agreement_ifd, point, sizeof(point), &point_len) != 0) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }

    error = emrtd_pace_general_authenticate(
        io, EMRTD_PACE_DO_KA_IFD, point, point_len, true, EMRTD_PACE_DO_KA_IC, &peer, &peer_len);
    if(error != EmrtdErrorNone) goto cleanup;

    if(emrtd_ec_point_read(&group, &agreement_ic, peer, peer_len) != 0) {
        error = EmrtdErrorPaceFailed;
        goto cleanup;
    }
    if(mbedtls_ecp_mul(
           &group, &agreement_shared, &agreement_key, &agreement_ic, emrtd_random_mbedtls, NULL) !=
           0 ||
       emrtd_ec_point_x(&group, &agreement_shared, curve->size, shared_x) != 0) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }
    if(!emrtd_kdf_enc_mac(info->cipher, shared_x, curve->size, ks_enc, ks_mac)) {
        error = EmrtdErrorInternal;
        goto cleanup;
    }

    /*
     * Step 5: the tokens. Both are built before the last exchange, because the
     * chip's point is still in the response buffer and the terminal's is still
     * in @c point; the next round overwrites both.
     */
    error = emrtd_pace_token_input(
        info->oid,
        info->oid_len,
        peer,
        peer_len,
        io->token_input,
        sizeof(io->token_input),
        &token_input_len);
    if(error != EmrtdErrorNone) goto cleanup;
    error = emrtd_pace_token(
        info->cipher, ks_mac, io->token_input, token_input_len, sizeof(io->token_input), token_ifd);
    if(error != EmrtdErrorNone) goto cleanup;

    error = emrtd_pace_token_input(
        info->oid,
        info->oid_len,
        point,
        point_len,
        io->token_input,
        sizeof(io->token_input),
        &token_input_len);
    if(error != EmrtdErrorNone) goto cleanup;
    error = emrtd_pace_token(
        info->cipher,
        ks_mac,
        io->token_input,
        token_input_len,
        sizeof(io->token_input),
        token_expected);
    if(error != EmrtdErrorNone) goto cleanup;

    /* The last round is not chained: it completes the command. */
    error = emrtd_pace_general_authenticate(
        io,
        EMRTD_PACE_DO_TOKEN_IFD,
        token_ifd,
        sizeof(token_ifd),
        false,
        EMRTD_PACE_DO_TOKEN_IC,
        &peer,
        &peer_len);
    if(error != EmrtdErrorNone) goto cleanup;

    if(peer_len != sizeof(token_expected) ||
       !emrtd_pace_equal(peer, token_expected, sizeof(token_expected))) {
        /*
         * The chip proved it holds a different password. Nothing was read and
         * nothing was written, so the document is no worse for it.
         */
        error = EmrtdErrorPaceFailed;
        goto cleanup;
    }

    /* PACE starts its Secure Messaging counter at zero (9303-11, 9.8.2). */
    emrtd_secure_wipe(ssc, sizeof(ssc));
    emrtd_sm_init(out_session, info->cipher, ks_enc, ks_mac, ssc);
    if(!out_session->established) {
        error = EmrtdErrorInternal;
    }

cleanup:
    mbedtls_ecp_point_free(&agreement_shared);
    mbedtls_ecp_point_free(&agreement_ic);
    mbedtls_ecp_point_free(&agreement_ifd);
    mbedtls_ecp_point_free(&generator);
    mbedtls_ecp_point_free(&mapping_shared);
    mbedtls_ecp_point_free(&mapping_ic);
    mbedtls_ecp_point_free(&mapping_ifd);
    mbedtls_mpi_free(&agreement_key);
    mbedtls_mpi_free(&mapping_key);
    mbedtls_mpi_free(&one);
    mbedtls_mpi_free(&s);
    /* Frees the domain parameters and the table mbedtls_ecp_mul() cached in it. */
    mbedtls_ecp_group_free(&group);

    emrtd_secure_wipe(kpi, sizeof(kpi));
    emrtd_secure_wipe(nonce, sizeof(nonce));
    emrtd_secure_wipe(shared_x, sizeof(shared_x));
    emrtd_secure_wipe(ks_enc, sizeof(ks_enc));
    emrtd_secure_wipe(ks_mac, sizeof(ks_mac));
    emrtd_secure_wipe(token_ifd, sizeof(token_ifd));
    emrtd_secure_wipe(token_expected, sizeof(token_expected));
    if(io != NULL) {
        /* The token inputs carry no secret, but the buffer is wiped all the same. */
        emrtd_secure_wipe(io, sizeof(*io));
        free(io);
    }

    if(error != EmrtdErrorNone) {
        emrtd_sm_clear(out_session);
    }
    return error;
}
