/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Basic Access Control against the worked example of ICAO Doc 9303 part 11
 * appendix D.3.
 *
 * The example starts from the MRZ printed on the specimen page and carries on
 * to the session keys, so the whole chain is pinned here: the MRZ information,
 * Kseed, the two access keys, the EXTERNAL AUTHENTICATE payload, and finally
 * the session that appendix D.4 goes on to use.
 */

#include "emrtd_test.h"

#include <mbedtls/des.h>

#include "../../crypto/emrtd_bac.h"
#include "../../crypto/emrtd_mac.h"

#define D3_K_ENC   "AB94FDECF2674FDFB9B391F85D7F76F2"
#define D3_K_MAC   "7962D9ECE03D1ACD4C76089DCE131543"
#define D3_RND_IC  "4608F91988702212"
#define D3_RND_IFD "781723860C06C226"
#define D3_K_IFD   "0B795240CB7049B01C19B33E32804F0B"
#define D3_RESPONSE \
    "46B9342A41396CD7386BF5803104D7CEDC122B9132139BAF2EEDC94EE178534F2F2D235D074D7449"

static EmrtdCredentials specimen(void) {
    /* The ICAO specimen: Anna Maria Eriksson of Utopia. */
    EmrtdCredentials credentials;
    memset(&credentials, 0, sizeof(credentials));
    strcpy(credentials.document_number, "L898902C");
    strcpy(credentials.date_of_birth, "690806");
    strcpy(credentials.date_of_expiry, "940623");
    return credentials;
}

static void test_key_derivation(void) {
    emrtd_test_begin("appendix D.3: the access keys come out of the MRZ");

    const EmrtdCredentials credentials = specimen();
    uint8_t k_enc[16];
    uint8_t k_mac[16];
    TEST_EQ_INT(emrtd_bac_derive_keys(&credentials, k_enc, k_mac), EmrtdErrorNone);
    TEST_EQ_HEX(k_enc, sizeof(k_enc), D3_K_ENC);
    TEST_EQ_HEX(k_mac, sizeof(k_mac), D3_K_MAC);

    emrtd_test_begin("a document number longer than nine characters still derives");
    /*
     * Appendix D.2's extended case: the number keeps its own check digit
     * instead of being padded out to nine characters, so it must produce a
     * different key from the specimen's.
     */
    EmrtdCredentials extended = credentials;
    strcpy(extended.document_number, "D23145890734");
    strcpy(extended.date_of_birth, "340712");
    strcpy(extended.date_of_expiry, "950712");
    uint8_t other_enc[16];
    uint8_t other_mac[16];
    TEST_EQ_INT(emrtd_bac_derive_keys(&extended, other_enc, other_mac), EmrtdErrorNone);
    TEST_CHECK(memcmp(other_enc, k_enc, sizeof(k_enc)) != 0);

    emrtd_test_begin("credentials without a document number are refused");
    EmrtdCredentials empty;
    memset(&empty, 0, sizeof(empty));
    TEST_CHECK(emrtd_bac_derive_keys(&empty, k_enc, k_mac) != EmrtdErrorNone);
    TEST_EQ_INT(emrtd_bac_derive_keys(NULL, k_enc, k_mac), EmrtdErrorInternal);
}

static void test_external_authenticate(void) {
    emrtd_test_begin("appendix D.3: the EXTERNAL AUTHENTICATE payload, byte for byte");

    uint8_t k_enc[16];
    uint8_t k_mac[16];
    uint8_t rnd_ic[8];
    uint8_t rnd_ifd[8];
    uint8_t k_ifd[16];
    emrtd_test_hex(D3_K_ENC, k_enc, sizeof(k_enc));
    emrtd_test_hex(D3_K_MAC, k_mac, sizeof(k_mac));
    emrtd_test_hex(D3_RND_IC, rnd_ic, sizeof(rnd_ic));
    emrtd_test_hex(D3_RND_IFD, rnd_ifd, sizeof(rnd_ifd));
    emrtd_test_hex(D3_K_IFD, k_ifd, sizeof(k_ifd));

    uint8_t payload[40];
    TEST_EQ_INT(
        emrtd_bac_build_external_auth(k_enc, k_mac, rnd_ic, rnd_ifd, k_ifd, payload),
        EmrtdErrorNone);
    TEST_EQ_HEX(
        payload,
        sizeof(payload),
        "72C29C2371CC9BDB65B779B8E8D37B29ECC154AA56A8799FAE2F498F76ED92F2"
        "5F1448EEA8AD90A7");

    emrtd_test_begin("ephemeral values left to the generator differ every time");
    uint8_t first[40];
    uint8_t second[40];
    TEST_EQ_INT(
        emrtd_bac_build_external_auth(k_enc, k_mac, rnd_ic, NULL, NULL, first), EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_bac_build_external_auth(k_enc, k_mac, rnd_ic, NULL, NULL, second), EmrtdErrorNone);
    TEST_CHECK(memcmp(first, second, sizeof(first)) != 0);
}

/** Rebuild a chip answer out of its three parts, so a single field can be changed. */
static void forge_response(
    const uint8_t k_enc[16],
    const uint8_t k_mac[16],
    const uint8_t plain[32],
    uint8_t out[40]) {
    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));
    mbedtls_des3_context des3;
    mbedtls_des3_init(&des3);
    mbedtls_des3_set2key_enc(&des3, k_enc);
    mbedtls_des3_crypt_cbc(&des3, MBEDTLS_DES_ENCRYPT, 32, iv, plain, out);
    mbedtls_des3_free(&des3);

    uint8_t padded[40];
    memcpy(padded, out, 32);
    const size_t padded_len = emrtd_pad_iso9797_m2(padded, 32, 8);
    emrtd_retail_mac(k_mac, padded, padded_len, out + 32);
}

static void test_response_processing(void) {
    emrtd_test_begin("appendix D.3: the chip's answer opens the session");

    uint8_t k_enc[16];
    uint8_t k_mac[16];
    uint8_t rnd_ic[8];
    uint8_t rnd_ifd[8];
    uint8_t k_ifd[16];
    uint8_t response[40];
    emrtd_test_hex(D3_K_ENC, k_enc, sizeof(k_enc));
    emrtd_test_hex(D3_K_MAC, k_mac, sizeof(k_mac));
    emrtd_test_hex(D3_RND_IC, rnd_ic, sizeof(rnd_ic));
    emrtd_test_hex(D3_RND_IFD, rnd_ifd, sizeof(rnd_ifd));
    emrtd_test_hex(D3_K_IFD, k_ifd, sizeof(k_ifd));
    emrtd_test_hex(D3_RESPONSE, response, sizeof(response));

    EmrtdSm session;
    TEST_EQ_INT(
        emrtd_bac_process_response(
            k_enc, k_mac, response, sizeof(response), rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    TEST_EQ_INT(session.cipher, EmrtdCipherTdes);
    TEST_EQ_HEX(session.ks_enc, 16, "979EC13B1CBFE9DCD01AB0FED307EAE5");
    TEST_EQ_HEX(session.ks_mac, 16, "F1CB1F1FB5ADF208806B89DC579DC1F8");
    /* The counter is the low half of each challenge, RND.IC first (9.8.2). */
    TEST_EQ_HEX(session.ssc, 8, "887022120C06C226");
    emrtd_sm_clear(&session);

    emrtd_test_begin("a tampered checksum is caught before anything is decrypted");
    uint8_t tampered[40];
    memcpy(tampered, response, sizeof(tampered));
    tampered[39] ^= 0x01;
    TEST_EQ_INT(
        emrtd_bac_process_response(
            k_enc, k_mac, tampered, sizeof(tampered), rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorWrongKey);
    TEST_CHECK(!session.established);

    emrtd_test_begin("an answer that echoes the wrong challenge is refused");
    /*
     * A chip that passes the checksum but returns a different RND.IFD is not
     * answering our exchange, so the session must not be opened even though
     * the cryptogram is well formed.
     */
    uint8_t plain[32];
    memcpy(plain, rnd_ic, 8);
    memcpy(plain + 8, rnd_ifd, 8);
    memset(plain + 16, 0x5A, 16);
    plain[8] ^= 0x01;
    uint8_t forged[40];
    forge_response(k_enc, k_mac, plain, forged);
    TEST_EQ_INT(
        emrtd_bac_process_response(
            k_enc, k_mac, forged, sizeof(forged), rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorWrongKey);

    emrtd_test_begin("the same answer with the right challenge opens a session");
    plain[8] ^= 0x01;
    forge_response(k_enc, k_mac, plain, forged);
    TEST_EQ_INT(
        emrtd_bac_process_response(
            k_enc, k_mac, forged, sizeof(forged), rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorNone);
    emrtd_sm_clear(&session);

    emrtd_test_begin("an answer of the wrong length is refused");
    TEST_EQ_INT(
        emrtd_bac_process_response(k_enc, k_mac, response, 39, rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorProtocol);
    TEST_EQ_INT(
        emrtd_bac_process_response(k_enc, k_mac, response, 0, rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorProtocol);
    TEST_EQ_INT(
        emrtd_bac_process_response(NULL, k_mac, response, 40, rnd_ic, rnd_ifd, k_ifd, &session),
        EmrtdErrorInternal);
}

void test_suite_bac(void) {
    test_key_derivation();
    test_external_authenticate();
    test_response_processing();
}
