/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_bac.h"

#include <string.h>

#include <mbedtls/des.h>
#include <mbedtls/sha1.h>

#include "../protocol/emrtd_mrz.h"
#include "emrtd_kdf.h"
#include "emrtd_mac.h"
#include "emrtd_rng.h"

#include "../emrtd_wipe.h"

/** S is RND.IFD || RND.IC || K.IFD, and E.IFD is its encryption (9303-11, 4.3.3). */
#define EMRTD_BAC_S_SIZE       32
/** E.IFD || M.IFD, and the chip's answer E.IC || M.IC, are both this long. */
#define EMRTD_BAC_PAYLOAD_SIZE 40

/** BAC encrypts with a zero initialisation vector; only the session uses a counter. */
static const uint8_t emrtd_bac_zero_iv[8] = {0};

/** Compare without letting the time taken reveal where two values first differ. */
static bool emrtd_bac_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for(size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/** Two key 3DES in CBC mode with a zero IV, in either direction. */
static bool emrtd_bac_crypt(
    const uint8_t key[16],
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t iv[8];
    memcpy(iv, emrtd_bac_zero_iv, sizeof(iv));

    mbedtls_des3_context des3;
    mbedtls_des3_init(&des3);
    const int keyed = encrypt ? mbedtls_des3_set2key_enc(&des3, key) :
                                mbedtls_des3_set2key_dec(&des3, key);
    const bool ok =
        keyed == 0 &&
        mbedtls_des3_crypt_cbc(
            &des3, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT, len, iv, in, out) == 0;
    mbedtls_des3_free(&des3);
    memset(iv, 0, sizeof(iv));
    return ok;
}

/** The checksum BAC puts on a cryptogram: a Retail MAC over the padded bytes. */
static bool emrtd_bac_mac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t mac[8]) {
    uint8_t padded[EMRTD_BAC_S_SIZE + 8];
    if(emrtd_padded_len(len, 8) > sizeof(padded)) {
        return false;
    }
    memcpy(padded, data, len);
    const size_t padded_len = emrtd_pad_iso9797_m2(padded, len, 8);
    const bool ok = emrtd_retail_mac(key, padded, padded_len, mac);
    emrtd_secure_wipe(padded, sizeof(padded));
    return ok;
}

EmrtdError emrtd_bac_derive_keys(
    const EmrtdCredentials* credentials,
    uint8_t out_k_enc[16],
    uint8_t out_k_mac[16]) {
    if(credentials == NULL || out_k_enc == NULL || out_k_mac == NULL) {
        return EmrtdErrorInternal;
    }

    char information[EMRTD_MRZ_INFO_MAX];
    EmrtdError error = emrtd_mrz_information(credentials, information, sizeof(information));
    if(error != EmrtdErrorNone) {
        return error;
    }

    /*
     * ICAO 9303-11, 4.3.2: Kseed is the first sixteen bytes of the SHA-1
     * digest of the MRZ information. The remaining four bytes of the digest
     * are discarded, not folded in.
     */
    uint8_t digest[20];
    uint8_t kseed[16];
    const int hashed =
        mbedtls_sha1((const unsigned char*)information, strlen(information), digest);
    if(hashed == 0) {
        memcpy(kseed, digest, sizeof(kseed));
        if(!emrtd_kdf_enc_mac(EmrtdCipherTdes, kseed, sizeof(kseed), out_k_enc, out_k_mac)) {
            error = EmrtdErrorInternal;
        }
    } else {
        error = EmrtdErrorInternal;
    }

    emrtd_secure_wipe(information, sizeof(information));
    emrtd_secure_wipe(digest, sizeof(digest));
    emrtd_secure_wipe(kseed, sizeof(kseed));
    if(error != EmrtdErrorNone) {
        emrtd_secure_wipe(out_k_enc, 16);
        emrtd_secure_wipe(out_k_mac, 16);
    }
    return error;
}

EmrtdError emrtd_bac_build_external_auth(
    const uint8_t k_enc[16],
    const uint8_t k_mac[16],
    const uint8_t rnd_ic[8],
    uint8_t rnd_ifd[8],
    uint8_t k_ifd[16],
    uint8_t out[40]) {
    if(k_enc == NULL || k_mac == NULL || rnd_ic == NULL || out == NULL) {
        return EmrtdErrorInternal;
    }

    /*
     * The two ephemeral values are ordinary inputs, which is what lets the
     * appendix D.3 vector pin this function. A caller that passes NULL has
     * them drawn here instead - and then cannot check the chip's answer,
     * because emrtd_bac_process_response() needs the same two values. The
     * driver therefore supplies its own buffers.
     */
    uint8_t rnd_ifd_local[8];
    uint8_t k_ifd_local[16];
    if(rnd_ifd == NULL) {
        emrtd_random_fill(rnd_ifd_local, sizeof(rnd_ifd_local));
        rnd_ifd = rnd_ifd_local;
    }
    if(k_ifd == NULL) {
        emrtd_random_fill(k_ifd_local, sizeof(k_ifd_local));
        k_ifd = k_ifd_local;
    }

    uint8_t s[EMRTD_BAC_S_SIZE];
    memcpy(s, rnd_ifd, 8);
    memcpy(s + 8, rnd_ic, 8);
    memcpy(s + 16, k_ifd, 16);

    EmrtdError error = EmrtdErrorNone;
    if(!emrtd_bac_crypt(k_enc, true, s, sizeof(s), out)) {
        error = EmrtdErrorInternal;
    } else if(!emrtd_bac_mac(k_mac, out, EMRTD_BAC_S_SIZE, out + EMRTD_BAC_S_SIZE)) {
        error = EmrtdErrorInternal;
    }

    emrtd_secure_wipe(s, sizeof(s));
    emrtd_secure_wipe(rnd_ifd_local, sizeof(rnd_ifd_local));
    emrtd_secure_wipe(k_ifd_local, sizeof(k_ifd_local));
    if(error != EmrtdErrorNone) {
        emrtd_secure_wipe(out, EMRTD_BAC_PAYLOAD_SIZE);
    }
    return error;
}

EmrtdError emrtd_bac_process_response(
    const uint8_t k_enc[16],
    const uint8_t k_mac[16],
    const uint8_t* response,
    size_t response_len,
    const uint8_t rnd_ic[8],
    const uint8_t rnd_ifd[8],
    const uint8_t k_ifd[16],
    EmrtdSm* out_session) {
    if(k_enc == NULL || k_mac == NULL || response == NULL || rnd_ic == NULL || rnd_ifd == NULL ||
       k_ifd == NULL || out_session == NULL) {
        return EmrtdErrorInternal;
    }
    if(response_len != EMRTD_BAC_PAYLOAD_SIZE) {
        return EmrtdErrorProtocol;
    }

    const uint8_t* const e_ic = response;
    const uint8_t* const m_ic = response + EMRTD_BAC_S_SIZE;

    /*
     * The checksum is verified before the cryptogram is touched. Both keys
     * come from the MRZ, so a failure here is almost always an MRZ that does
     * not belong to this document rather than a corrupted exchange.
     */
    uint8_t expected_mac[8];
    if(!emrtd_bac_mac(k_mac, e_ic, EMRTD_BAC_S_SIZE, expected_mac)) {
        return EmrtdErrorInternal;
    }
    if(!emrtd_bac_equal(expected_mac, m_ic, sizeof(expected_mac))) {
        emrtd_secure_wipe(expected_mac, sizeof(expected_mac));
        return EmrtdErrorWrongKey;
    }
    emrtd_secure_wipe(expected_mac, sizeof(expected_mac));

    EmrtdError error = EmrtdErrorNone;
    uint8_t r[EMRTD_BAC_S_SIZE];
    uint8_t kseed[16];
    uint8_t ks_enc[16];
    uint8_t ks_mac[16];
    uint8_t ssc[8];

    if(!emrtd_bac_crypt(k_enc, false, e_ic, EMRTD_BAC_S_SIZE, r)) {
        error = EmrtdErrorInternal;
    } else if(!emrtd_bac_equal(r, rnd_ic, 8) || !emrtd_bac_equal(r + 8, rnd_ifd, 8)) {
        /* The chip echoed values we never sent, so the two sides do not agree. */
        error = EmrtdErrorWrongKey;
    } else {
        /* Kseed is the exclusive or of the two halves (9303-11, 4.3.3 step 5). */
        for(size_t i = 0; i < sizeof(kseed); i++) {
            kseed[i] = (uint8_t)(k_ifd[i] ^ r[16 + i]);
        }
        if(!emrtd_kdf_enc_mac(EmrtdCipherTdes, kseed, sizeof(kseed), ks_enc, ks_mac)) {
            error = EmrtdErrorInternal;
        } else {
            /* The session starts at the low halves of the two challenges (9.8.2). */
            memcpy(ssc, rnd_ic + 4, 4);
            memcpy(ssc + 4, rnd_ifd + 4, 4);
            emrtd_sm_init(out_session, EmrtdCipherTdes, ks_enc, ks_mac, ssc);
        }
    }

    emrtd_secure_wipe(r, sizeof(r));
    emrtd_secure_wipe(kseed, sizeof(kseed));
    emrtd_secure_wipe(ks_enc, sizeof(ks_enc));
    emrtd_secure_wipe(ks_mac, sizeof(ks_mac));
    emrtd_secure_wipe(ssc, sizeof(ssc));
    return error;
}
