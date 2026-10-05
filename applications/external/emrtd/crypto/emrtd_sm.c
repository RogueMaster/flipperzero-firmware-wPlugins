/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_sm.h"

#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/des.h>

#include "../protocol/emrtd_tlv.h"
#include "../transport/emrtd_transceiver.h"
#include "emrtd_mac.h"

#include "../emrtd_wipe.h"

/* The data objects of a protected APDU, ISO/IEC 7816-4 table 49. */
#define EMRTD_DO_CRYPTOGRAM_PADDED 0x87 /**< Cryptogram with a padding indicator. */
#define EMRTD_DO_CRYPTOGRAM_PLAIN  0x85 /**< Cryptogram without one. */
#define EMRTD_DO_LE                0x97
#define EMRTD_DO_STATUS            0x99
#define EMRTD_DO_MAC               0x8E

/** The one padding indicator ICAO 9303-11 uses: padding per ISO/IEC 9797-1 method 2. */
#define EMRTD_PADDING_INDICATOR 0x01

/** Class byte bit that marks an APDU as protected (ISO/IEC 7816-4, 5.1.1). */
#define EMRTD_CLA_SM 0x0C

/*
 * A protected command has to reach the card in one frame, so its data field is
 * bounded by the largest APDU less the header, Lc and Le.
 */
#define EMRTD_SM_PAYLOAD_MAX (EMRTD_APDU_MAX_SIZE - 6)

/*
 * The MAC is taken over SSC || M with ISO/IEC 9797-1 method 2 padding. For a
 * command M is the padded header followed by the data objects; for a response
 * it is everything the card sent ahead of DO'8E'. Both stay inside this.
 */
#define EMRTD_SM_MAC_INPUT_MAX \
    (2 * EMRTD_BLOCK_MAX_SIZE + EMRTD_APDU_MAX_SIZE + EMRTD_BLOCK_MAX_SIZE)

/**
 * Compare two secrets without letting the time taken depend on where they
 * first differ.
 *
 * Used on every value an attacker could iterate towards: the checksum of a
 * response, and by the access protocols the tokens of their peers.
 */
static bool emrtd_sm_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for(size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/**
 * Increment the Send Sequence Counter, ICAO 9303-11 section 9.8.2.
 *
 * The counter is a big endian integer as wide as the cipher block, and it
 * wraps rather than saturating.
 */
static void emrtd_sm_increment_ssc(EmrtdSm* sm) {
    const size_t block = emrtd_cipher_block_size(sm->cipher);
    for(size_t i = block; i > 0; i--) {
        if(++sm->ssc[i - 1] != 0) {
            break;
        }
    }
}

/**
 * Encrypt or decrypt a whole number of blocks in CBC mode.
 *
 * The initialisation vector is a property of the cipher family: 3DES uses a
 * zero IV, while AES uses E(KSEnc, SSC) in ECB mode (9.8.7.1), which is why
 * the counter has to be incremented before this is called.
 *
 * @p in and @p out may be the same buffer, and @p out may lie below @p in
 * within one buffer: both cipher implementations read a whole block before
 * they write it, so an overlap in that direction is safe. That is what lets
 * emrtd_sm_unprotect() decrypt a response over itself.
 */
static EmrtdError
    emrtd_sm_crypt(const EmrtdSm* sm, bool encrypt, const uint8_t* in, size_t len, uint8_t* out) {
    const size_t block = emrtd_cipher_block_size(sm->cipher);
    if(len == 0 || len % block != 0) {
        return EmrtdErrorSecureMessaging;
    }

    EmrtdError error = EmrtdErrorNone;
    uint8_t iv[EMRTD_BLOCK_MAX_SIZE];
    memset(iv, 0, sizeof(iv));

    if(sm->cipher == EmrtdCipherTdes) {
        mbedtls_des3_context des3;
        mbedtls_des3_init(&des3);
        const int ret = encrypt ? mbedtls_des3_set2key_enc(&des3, sm->ks_enc) :
                                  mbedtls_des3_set2key_dec(&des3, sm->ks_enc);
        if(ret != 0 ||
           mbedtls_des3_crypt_cbc(
               &des3, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT, len, iv, in, out) !=
               0) {
            error = EmrtdErrorSecureMessaging;
        }
        mbedtls_des3_free(&des3);
    } else {
        const unsigned int key_bits = (unsigned int)(emrtd_cipher_key_size(sm->cipher) * 8);

        /* The IV is derived with the encryption schedule in both directions. */
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        if(mbedtls_aes_setkey_enc(&aes, sm->ks_enc, key_bits) != 0 ||
           mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, sm->ssc, iv) != 0) {
            error = EmrtdErrorSecureMessaging;
        }

        if(error == EmrtdErrorNone) {
            const int ret = encrypt ? mbedtls_aes_setkey_enc(&aes, sm->ks_enc, key_bits) :
                                      mbedtls_aes_setkey_dec(&aes, sm->ks_enc, key_bits);
            if(ret != 0 ||
               mbedtls_aes_crypt_cbc(
                   &aes, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, len, iv, in, out) !=
                   0) {
                error = EmrtdErrorSecureMessaging;
            }
        }
        mbedtls_aes_free(&aes);
    }

    memset(iv, 0, sizeof(iv));
    return error;
}

/**
 * Checksum SSC || M, which @p buf already holds in its first @p len bytes.
 *
 * The padding is applied in place, so @p buf needs room for a further block.
 * Both cipher families pad the MAC input with ISO/IEC 9797-1 method 2: that is
 * what the appendix D vectors show for 3DES, and what a real passport does for
 * AES. AES-CMAC would pad by itself, and padding it twice looks wrong until
 * one of the two is removed and a chip stops answering.
 */
static EmrtdError
    emrtd_sm_mac(const EmrtdSm* sm, uint8_t* buf, size_t len, size_t capacity, uint8_t* mac) {
    const size_t block = emrtd_cipher_block_size(sm->cipher);
    const size_t padded = emrtd_padded_len(len, block);
    if(padded > capacity) {
        return EmrtdErrorBufferTooSmall;
    }
    emrtd_pad_iso9797_m2(buf, len, block);

    const bool ok =
        sm->cipher == EmrtdCipherTdes ?
            emrtd_retail_mac(sm->ks_mac, buf, padded, mac) :
            emrtd_aes_cmac(
                sm->ks_mac, emrtd_cipher_key_size(sm->cipher), buf, padded, mac, EMRTD_MAC_SIZE);
    return ok ? EmrtdErrorNone : EmrtdErrorSecureMessaging;
}

/** Write a data object header, returning the bytes it took. */
static size_t emrtd_sm_write_do_header(uint8_t* out, uint8_t tag, size_t value_len) {
    out[0] = tag;
    return 1 + emrtd_tlv_encode_length(value_len, out + 1);
}

void emrtd_sm_init(
    EmrtdSm* sm,
    EmrtdCipher cipher,
    const uint8_t* ks_enc,
    const uint8_t* ks_mac,
    const uint8_t* ssc) {
    if(sm == NULL) {
        return;
    }
    emrtd_secure_wipe(sm, sizeof(*sm));

    const size_t key_size = emrtd_cipher_key_size(cipher);
    if(key_size == 0 || ks_enc == NULL || ks_mac == NULL) {
        /* Leaves the session unestablished, so every later call refuses. */
        return;
    }

    sm->cipher = cipher;
    memcpy(sm->ks_enc, ks_enc, key_size);
    memcpy(sm->ks_mac, ks_mac, key_size);
    if(ssc != NULL) {
        memcpy(sm->ssc, ssc, emrtd_cipher_block_size(cipher));
    }
    sm->established = true;
}

void emrtd_sm_clear(EmrtdSm* sm) {
    if(sm == NULL) {
        return;
    }
    emrtd_secure_wipe(sm, sizeof(*sm));
}

size_t emrtd_sm_command_overhead(const EmrtdSm* sm, size_t data_len) {
    /*
     * The worst case growth of the command's data field: the cryptogram in
     * place of the plaintext, DO'97' (assumed present, because a command
     * without an expected length is the exception), and DO'8E'.
     */
    if(sm == NULL || !sm->established) {
        return 0;
    }
    const size_t do97 = 2 + 1;
    const size_t do8e = 2 + EMRTD_MAC_SIZE;
    if(data_len == 0) {
        return do97 + do8e;
    }

    const size_t value_len = 1 + emrtd_padded_len(data_len, emrtd_cipher_block_size(sm->cipher));
    const size_t do87 = 1 + emrtd_tlv_encoded_length_size(value_len) + value_len;
    return (do87 - data_len) + do97 + do8e;
}

EmrtdError emrtd_sm_protect(
    EmrtdSm* sm,
    const EmrtdCommandApdu* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(sm == NULL || command == NULL || out == NULL || out_len == NULL) {
        return EmrtdErrorInternal;
    }

    /* Nothing was written, whichever way this call ends. */
    *out_len = 0;

    if(!sm->established) {
        return EmrtdErrorSecureMessaging;
    }
    if(command->data_len != 0 && command->data == NULL) {
        return EmrtdErrorInternal;
    }
    if(command->le != EMRTD_LE_NONE && (command->le < 1 || command->le > 65536)) {
        return EmrtdErrorInvalidInput;
    }

    const size_t block = emrtd_cipher_block_size(sm->cipher);

    /* Lay the envelope out before anything is written, so every size is checked first. */
    size_t cryptogram_len = 0;
    size_t do87_len = 0;
    if(command->data_len != 0) {
        cryptogram_len = emrtd_padded_len(command->data_len, block);
        const size_t value_len = 1 + cryptogram_len;
        do87_len = 1 + emrtd_tlv_encoded_length_size(value_len) + value_len;
    }

    size_t do97_len = 0;
    if(command->le != EMRTD_LE_NONE) {
        do97_len = command->le <= EMRTD_LE_MAX ? 3 : 4;
    }

    const size_t do8e_len = 2 + EMRTD_MAC_SIZE;
    const size_t payload_len = do87_len + do97_len + do8e_len;
    if(payload_len > EMRTD_SM_PAYLOAD_MAX) {
        return EmrtdErrorBufferTooSmall;
    }

    /* SSC, the padded header, and the data objects the checksum covers. */
    const size_t mac_input_len = block + block + do87_len + do97_len;
    if(emrtd_padded_len(mac_input_len, block) > EMRTD_SM_MAC_INPUT_MAX) {
        return EmrtdErrorBufferTooSmall;
    }

    /*
     * The caller's buffer is measured here rather than left to the encoder,
     * because by then the counter has moved. A session whose counter has
     * advanced without a command reaching the chip cannot be recovered, and it
     * would be a poor way to learn that a buffer was a few bytes short.
     */
    if(4 + 1 + payload_len + 1 > out_size) {
        return EmrtdErrorBufferTooSmall;
    }

    /* Only now does the session state change; a rejected command leaves it intact. */
    emrtd_sm_increment_ssc(sm);

    EmrtdError error = EmrtdErrorNone;
    uint8_t mac_input[EMRTD_SM_MAC_INPUT_MAX];
    uint8_t payload[EMRTD_SM_PAYLOAD_MAX];
    uint8_t mac[EMRTD_MAC_SIZE];

    memcpy(mac_input, sm->ssc, block);

    /*
     * ICAO 9303-11, 9.8.3: the header the checksum covers is the one actually
     * sent, class byte included, padded to a block.
     */
    uint8_t* const header = mac_input + block;
    header[0] = (uint8_t)(command->cla | EMRTD_CLA_SM);
    header[1] = command->ins;
    header[2] = command->p1;
    header[3] = command->p2;
    emrtd_pad_iso9797_m2(header, 4, block);

    uint8_t* const objects = mac_input + block + block;
    size_t offset = 0;

    if(do87_len != 0) {
        offset += emrtd_sm_write_do_header(
            objects + offset, EMRTD_DO_CRYPTOGRAM_PADDED, 1 + cryptogram_len);
        objects[offset++] = EMRTD_PADDING_INDICATOR;

        /* Pad the plaintext where it will be encrypted, then encrypt it in place. */
        memcpy(objects + offset, command->data, command->data_len);
        emrtd_pad_iso9797_m2(objects + offset, command->data_len, block);
        error = emrtd_sm_crypt(sm, true, objects + offset, cryptogram_len, objects + offset);
        offset += cryptogram_len;
    }

    if(error == EmrtdErrorNone && do97_len != 0) {
        const size_t value_len = do97_len - 2;
        offset += emrtd_sm_write_do_header(objects + offset, EMRTD_DO_LE, value_len);
        if(value_len == 1) {
            /* Le = 256 is the short encoding's zero (ISO/IEC 7816-4, 5.1). */
            objects[offset++] =
                (uint8_t)(command->le == EMRTD_LE_MAX ? 0x00 : (command->le & 0xFF));
        } else {
            const uint32_t le = command->le >= 65536 ? 0u : (uint32_t)command->le;
            objects[offset++] = (uint8_t)(le >> 8);
            objects[offset++] = (uint8_t)(le & 0xFF);
        }
    }

    if(error == EmrtdErrorNone) {
        memcpy(payload, objects, offset);
        error = emrtd_sm_mac(sm, mac_input, block + block + offset, sizeof(mac_input), mac);
    }

    if(error == EmrtdErrorNone) {
        size_t payload_used = offset;
        payload_used +=
            emrtd_sm_write_do_header(payload + payload_used, EMRTD_DO_MAC, sizeof(mac));
        memcpy(payload + payload_used, mac, sizeof(mac));
        payload_used += sizeof(mac);

        /*
         * The answer carries an envelope of its own, so the protected command
         * always asks for the largest short response; what the caller wanted
         * travelled in DO'97'.
         */
        const EmrtdCommandApdu protected_command = {
            .cla = (uint8_t)(command->cla | EMRTD_CLA_SM),
            .ins = command->ins,
            .p1 = command->p1,
            .p2 = command->p2,
            .data = payload,
            .data_len = payload_used,
            .le = EMRTD_LE_MAX,
        };
        error = emrtd_apdu_encode(&protected_command, out, out_size, out_len);
    }

    emrtd_secure_wipe(mac_input, sizeof(mac_input));
    emrtd_secure_wipe(payload, sizeof(payload));
    emrtd_secure_wipe(mac, sizeof(mac));
    if(error != EmrtdErrorNone) {
        *out_len = 0;
    }
    return error;
}

EmrtdError emrtd_sm_unprotect(
    EmrtdSm* sm,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_size,
    EmrtdResponseApdu* out_apdu) {
    if(sm == NULL || response == NULL || (out == NULL && out_size != 0)) {
        return EmrtdErrorInternal;
    }
    if(!sm->established) {
        return EmrtdErrorSecureMessaging;
    }
    if(response_len < 2 || response_len > EMRTD_APDU_MAX_SIZE) {
        return EmrtdErrorSecureMessaging;
    }

    const uint16_t outer_sw =
        (uint16_t)((response[response_len - 2] << 8) | response[response_len - 1]);
    if(out_apdu != NULL) {
        out_apdu->data = NULL;
        out_apdu->data_len = 0;
        out_apdu->sw = outer_sw;
    }

    /*
     * Walk the data objects that precede the status word. The three are only
     * ever read once their flag is set, but they are cleared anyway: the
     * compiler cannot follow that reasoning, and a response object that is
     * empty rather than stale is the safer thing to have on the stack.
     */
    EmrtdTlv cryptogram = {0};
    EmrtdTlv status = {0};
    EmrtdTlv checksum = {0};
    bool has_cryptogram = false;
    bool has_status = false;
    bool has_checksum = false;

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_init(&iter, response, response_len - 2);
    while(emrtd_tlv_iter_next(&iter, &node)) {
        switch(node.tag) {
        case EMRTD_DO_CRYPTOGRAM_PADDED:
        case EMRTD_DO_CRYPTOGRAM_PLAIN:
            if(has_cryptogram) return EmrtdErrorSecureMessaging;
            cryptogram = node;
            has_cryptogram = true;
            break;
        case EMRTD_DO_STATUS:
            if(has_status) return EmrtdErrorSecureMessaging;
            status = node;
            has_status = true;
            break;
        case EMRTD_DO_MAC:
            if(has_checksum) return EmrtdErrorSecureMessaging;
            checksum = node;
            has_checksum = true;
            break;
        default:
            /* An object this reader does not know is still covered by the MAC. */
            break;
        }
    }
    if(!emrtd_tlv_iter_exhausted(&iter)) {
        return EmrtdErrorSecureMessaging;
    }

    if(!has_checksum) {
        /*
         * The card answered without a checksum, which happens when it refuses
         * the command outright - 6987 or 6988 when the envelope itself was
         * wrong, or a plain error from a chip that gave up on the session. The
         * outer status word is the only thing it told us, so that is what the
         * caller is given; the counter is left alone, exactly as it is on the
         * card, in case the caller can still make sense of the session.
         */
        return outer_sw == 0x9000 ? EmrtdErrorSecureMessaging : emrtd_error_from_sw(outer_sw);
    }

    /* DO'8E' has to be the last object, or the checksum would not cover the rest. */
    if(checksum.value_len != EMRTD_MAC_SIZE ||
       checksum.start + checksum.total_len != response + (response_len - 2)) {
        return EmrtdErrorSecureMessaging;
    }
    if(has_status && status.value_len != 2) {
        return EmrtdErrorSecureMessaging;
    }

    const size_t block = emrtd_cipher_block_size(sm->cipher);
    size_t cryptogram_len = 0;
    const uint8_t* ciphertext = NULL;
    if(has_cryptogram) {
        ciphertext = cryptogram.value;
        cryptogram_len = cryptogram.value_len;
        if(cryptogram.tag == EMRTD_DO_CRYPTOGRAM_PADDED) {
            /* ICAO 9303-11 knows exactly one indicator; anything else is not our cipher. */
            if(cryptogram_len < 1 || ciphertext[0] != EMRTD_PADDING_INDICATOR) {
                return EmrtdErrorSecureMessaging;
            }
            ciphertext++;
            cryptogram_len--;
        }
        if(cryptogram_len == 0 || cryptogram_len % block != 0) {
            return EmrtdErrorSecureMessaging;
        }
        if(cryptogram_len > out_size) {
            return EmrtdErrorBufferTooSmall;
        }
    }

    /*
     * The checksum covers SSC followed by every byte the card sent ahead of
     * DO'8E'. Taking those bytes as they arrived, rather than re-encoding the
     * objects, keeps the reader honest about a card that spells a length
     * differently from the way this code would have.
     */
    const size_t covered = (size_t)(checksum.start - response);
    uint8_t mac_input[EMRTD_SM_MAC_INPUT_MAX];
    if(block + covered + block > sizeof(mac_input)) {
        return EmrtdErrorBufferTooSmall;
    }

    emrtd_sm_increment_ssc(sm);
    memcpy(mac_input, sm->ssc, block);
    memcpy(mac_input + block, response, covered);

    uint8_t mac[EMRTD_MAC_SIZE];
    EmrtdError error = emrtd_sm_mac(sm, mac_input, block + covered, sizeof(mac_input), mac);
    if(error == EmrtdErrorNone && !emrtd_sm_equal(mac, checksum.value, sizeof(mac))) {
        error = EmrtdErrorSecureMessaging;
    }
    emrtd_secure_wipe(mac_input, sizeof(mac_input));
    emrtd_secure_wipe(mac, sizeof(mac));
    if(error != EmrtdErrorNone) {
        return error;
    }

    /*
     * Everything read out of the response has to be taken before the plaintext
     * is written, because @p out is allowed to be the response buffer itself.
     */
    const uint16_t sw = has_status ? (uint16_t)((status.value[0] << 8) | status.value[1]) :
                                     outer_sw;

    size_t plain_len = 0;
    if(has_cryptogram) {
        error = emrtd_sm_crypt(sm, false, ciphertext, cryptogram_len, out);
        if(error == EmrtdErrorNone && !emrtd_unpad_iso9797_m2(out, cryptogram_len, &plain_len)) {
            /* A missing 0x80 marker means the plaintext is not what we think it is. */
            error = EmrtdErrorSecureMessaging;
        }
        if(error != EmrtdErrorNone) {
            emrtd_secure_wipe(out, cryptogram_len);
            return error;
        }
    }

    if(out_apdu != NULL) {
        out_apdu->data = plain_len != 0 ? out : NULL;
        out_apdu->data_len = plain_len;
        out_apdu->sw = sw;
    }
    return EmrtdErrorNone;
}
