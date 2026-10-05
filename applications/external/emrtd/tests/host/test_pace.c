/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * PACE with the generic mapping over ECDH.
 *
 * Two kinds of test sit here. The first are the ICAO Doc 9303 part 11
 * appendix G vectors, which pin the parts of the protocol that can be computed
 * without a chip: the key derived from the password, and the input and value
 * of an authentication token.
 *
 * The second drive the whole exchange against the simulated chip, which runs
 * the protocol independently. Those cover what a vector cannot reach - the
 * order of the four General Authenticate rounds, command chaining, the data
 * object framing, and what happens when the password is wrong. Both sides have
 * to agree on every byte for the exchange to complete at all.
 */

#include "emrtd_test.h"

#include "../../crypto/emrtd_mac.h"
#include "../../crypto/emrtd_pace.h"
#include "../../sim/emrtd_sim.h"

/** id-PACE-ECDH-GM-AES-CBC-CMAC-128, the protocol nearly every passport uses. */
#define PACE_OID_AES128 "04007F00070202040202"

static EmrtdCredentials
    credentials_for(const char* number, const char* birth, const char* expiry) {
    EmrtdCredentials credentials;
    memset(&credentials, 0, sizeof(credentials));
    strcpy(credentials.document_number, number);
    strcpy(credentials.date_of_birth, birth);
    strcpy(credentials.date_of_expiry, expiry);
    return credentials;
}

static void test_password_key(void) {
    emrtd_test_begin("appendix G: Kpi comes out of the MRZ information");

    /* The appendix G specimen, which is not the one appendix D uses. */
    const EmrtdCredentials credentials = credentials_for("T22000129", "640812", "101031");
    uint8_t kpi[16];
    TEST_EQ_INT(emrtd_pace_password_key(&credentials, kpi), EmrtdErrorNone);
    TEST_EQ_HEX(kpi, sizeof(kpi), "89DED1B26624EC1E634C1989302849DD");

    emrtd_test_begin("a Card Access Number is a password in its own right");
    EmrtdCredentials with_can = credentials;
    with_can.has_can = true;
    strcpy(with_can.can, "123456");
    uint8_t can_key[16];
    TEST_EQ_INT(emrtd_pace_password_key(&with_can, can_key), EmrtdErrorNone);
    /*
     * The CAN is taken as printed, not hashed, so it cannot produce the same
     * key as the MRZ of the same document.
     */
    TEST_CHECK(memcmp(can_key, kpi, sizeof(kpi)) != 0);

    emrtd_test_begin("credentials with neither password are refused");
    EmrtdCredentials empty;
    memset(&empty, 0, sizeof(empty));
    TEST_CHECK(emrtd_pace_password_key(&empty, kpi) != EmrtdErrorNone);
}

static void test_token(void) {
    emrtd_test_begin("appendix G: the token input is 7F49 { 06 oid 86 point }");

    uint8_t oid[16];
    uint8_t point[EMRTD_EC_POINT_MAX];
    const size_t oid_len = emrtd_test_hex(PACE_OID_AES128, oid, sizeof(oid));
    const size_t point_len = emrtd_test_hex(
        "049E880F842905B8B3181F7AF7CAA9F0EFB743847F44A306D2D28C1D9EC65DF6DB"
        "7764B22277A2EDDC3C265A9F018F9CB852E111B768B326904B59A0193776F094",
        point,
        sizeof(point));
    TEST_EQ_INT(point_len, 65);

    uint8_t input[160];
    size_t input_len = 0;
    TEST_EQ_INT(
        emrtd_pace_token_input(oid, oid_len, point, point_len, input, sizeof(input), &input_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(
        input,
        input_len,
        "7F494F060A04007F00070202040202864104"
        "9E880F842905B8B3181F7AF7CAA9F0EFB743847F44A306D2D28C1D9EC65DF6DB"
        "7764B22277A2EDDC3C265A9F018F9CB852E111B768B326904B59A0193776F094");

    emrtd_test_begin("appendix G: the token is the first eight bytes of its CMAC");
    uint8_t ks_mac[16];
    uint8_t token[8];
    emrtd_test_hex("FE251C7858B356B24514B3BD5F4297D1", ks_mac, sizeof(ks_mac));
    TEST_CHECK(emrtd_aes_cmac(ks_mac, sizeof(ks_mac), input, input_len, token, sizeof(token)));
    TEST_EQ_HEX(token, sizeof(token), "C2B0BD78D94BA866");

    emrtd_test_begin("a token input that will not fit is refused, not truncated");
    uint8_t tiny[16];
    TEST_EQ_INT(
        emrtd_pace_token_input(oid, oid_len, point, point_len, tiny, sizeof(tiny), &input_len),
        EmrtdErrorBufferTooSmall);
    TEST_EQ_INT(
        emrtd_pace_token_input(oid, 0, point, point_len, input, sizeof(input), &input_len),
        EmrtdErrorInvalidInput);
}

/* A port that fails loudly, to prove the refusals happen before the field is used. */
static EmrtdError never_transceive(
    void* ctx,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_cap;
    (void)rx_len;
    *(int*)ctx += 1;
    return EmrtdErrorTransport;
}

static const EmrtdTransceiverApi never_api = {.name = "unused", .transceive = never_transceive};

static void test_refusals_before_the_first_apdu(void) {
    emrtd_test_begin("an impossible protocol is named before a card is touched");

    int calls = 0;
    EmrtdTransceiver transceiver = {.api = &never_api, .ctx = &calls, .fsc = 256, .fsd = 256};

    const EmrtdCredentials credentials = credentials_for("L898902C", "690806", "300701");
    EmrtdSm session;
    memset(&session, 0, sizeof(session));

    EmrtdPaceInfo info;
    memset(&info, 0, sizeof(info));
    emrtd_test_hex(PACE_OID_AES128, info.oid, sizeof(info.oid));
    info.oid_len = 10;
    info.cipher = EmrtdCipherAes128;
    info.curve = emrtd_ec_curve_by_param_id(13);
    info.mapping = EmrtdPaceMappingGeneric;
    info.agreement = EmrtdPaceAgreementEcdh;
    info.usable = true;

    /* PACE over a MODP group cannot be computed at all; see docs/platform.md. */
    EmrtdPaceInfo variant = info;
    variant.agreement = EmrtdPaceAgreementDh;
    TEST_EQ_INT(
        emrtd_pace_run(&transceiver, &variant, &credentials, &session),
        EmrtdErrorPaceUnsupportedDh);

    variant = info;
    variant.mapping = EmrtdPaceMappingIntegrated;
    TEST_EQ_INT(
        emrtd_pace_run(&transceiver, &variant, &credentials, &session),
        EmrtdErrorPaceUnsupportedMapping);

    /* Id 17 is brainpoolP512r1, which is above MBEDTLS_ECP_MAX_BITS. */
    variant = info;
    variant.curve = emrtd_ec_curve_by_param_id(17);
    TEST_EQ_INT(
        emrtd_pace_run(&transceiver, &variant, &credentials, &session),
        EmrtdErrorPaceUnsupportedCurve);

    variant = info;
    variant.curve = NULL;
    TEST_EQ_INT(
        emrtd_pace_run(&transceiver, &variant, &credentials, &session),
        EmrtdErrorPaceUnsupportedCurve);

    emrtd_test_begin("credentials that cannot derive a key are refused too");
    EmrtdCredentials empty;
    memset(&empty, 0, sizeof(empty));
    TEST_EQ_INT(emrtd_pace_run(&transceiver, &info, &empty, &session), EmrtdErrorInvalidInput);

    /* Nothing above should have reached the card. */
    TEST_EQ_INT(calls, 0);
    TEST_CHECK(!session.established);
}

static EmrtdSim* pace_chip(EmrtdCipher cipher, uint8_t parameter_id) {
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessPaceOnly;
    config.credentials = credentials_for("L898902C", "690806", "300701");
    config.pace_cipher = cipher;
    config.pace_parameter_id = parameter_id;
    return emrtd_sim_alloc(&config);
}

/** Read EF.CardAccess in the clear and pull the PACEInfo out of it. */
static bool read_pace_info(EmrtdSim* chip, EmrtdPaceInfo* out) {
    size_t len = 0;
    const uint8_t* const card_access = emrtd_sim_file(chip, EMRTD_SIM_FID_CARD_ACCESS, &len);
    return card_access != NULL && emrtd_security_infos_best_pace(card_access, len, out);
}

static void test_exchange(void) {
    emrtd_test_begin("the four rounds open a session on brainpoolP256r1");

    EmrtdSim* chip = pace_chip(EmrtdCipherAes128, 13);
    TEST_CHECK(chip != NULL);

    EmrtdPaceInfo info;
    TEST_CHECK(read_pace_info(chip, &info));
    TEST_CHECK(info.usable);
    TEST_EQ_INT(info.cipher, EmrtdCipherAes128);
    TEST_EQ_STR(info.curve->name, "brainpoolP256r1");

    const EmrtdCredentials credentials = credentials_for("L898902C", "690806", "300701");
    EmrtdSm session;
    TEST_EQ_INT(
        emrtd_pace_run(emrtd_sim_transceiver(chip), &info, &credentials, &session),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    TEST_EQ_INT(session.cipher, EmrtdCipherAes128);
    /* PACE starts its counter at zero, unlike BAC. */
    TEST_EQ_HEX(session.ssc, 16, "00000000000000000000000000000000");
    TEST_CHECK(emrtd_sim_authenticated(chip));
    TEST_EQ_STR(emrtd_sim_protocol(chip), "PACE");

    /* MSE:Set AT and four General Authenticate rounds, and nothing else. */
    TEST_EQ_INT(emrtd_sim_exchanges(chip), 5);

    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);

    emrtd_test_begin("the session both sides derived really is the same one");
    /*
     * The tokens already proved the two sides agree on KSMAC. Sending one
     * protected command proves they agree on KSEnc and on the counter as well,
     * which is the part a token exchange cannot show.
     */
    chip = pace_chip(EmrtdCipherAes128, 13);
    TEST_CHECK(read_pace_info(chip, &info));
    TEST_EQ_INT(
        emrtd_pace_run(emrtd_sim_transceiver(chip), &info, &credentials, &session),
        EmrtdErrorNone);

    uint8_t fid[2] = {0x01, 0x1E};
    const EmrtdCommandApdu select = {
        .cla = 0x00,
        .ins = 0xA4,
        .p1 = 0x02,
        .p2 = 0x0C,
        .data = fid,
        .data_len = sizeof(fid),
        .le = EMRTD_LE_NONE,
    };
    uint8_t command[EMRTD_APDU_MAX_SIZE];
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    size_t command_len = 0;
    size_t response_len = 0;
    EmrtdResponseApdu parsed;
    TEST_EQ_INT(
        emrtd_sm_protect(&session, &select, command, sizeof(command), &command_len),
        EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(
            emrtd_sim_transceiver(chip),
            command,
            command_len,
            response,
            sizeof(response),
            &response_len),
        EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_sm_unprotect(&session, response, response_len, response, sizeof(response), &parsed),
        EmrtdErrorNone);
    TEST_EQ_INT(parsed.sw, 0x9000);

    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);
}

static void test_other_ciphers_and_curves(void) {
    emrtd_test_begin("AES-192 and AES-256 sessions need a Kpi of their own length");

    /*
     * Kpi belongs to the cipher the PACEInfo names (9303-11, 9.7.3), so a
     * protocol above AES-128 derives it with SHA-256 and more bytes. Were the
     * sixteen byte key used throughout, the nonce would decrypt to rubbish and
     * the exchange would fail here rather than in the field.
     */
    const EmrtdCredentials credentials = credentials_for("L898902C", "690806", "300701");
    const EmrtdCipher ciphers[] = {EmrtdCipherAes192, EmrtdCipherAes256};

    for(size_t i = 0; i < sizeof(ciphers) / sizeof(ciphers[0]); i++) {
        EmrtdSim* const chip = pace_chip(ciphers[i], 13);
        TEST_CHECK(chip != NULL);

        EmrtdPaceInfo info;
        TEST_CHECK(read_pace_info(chip, &info));
        TEST_EQ_INT(info.cipher, ciphers[i]);

        EmrtdSm session;
        TEST_EQ_INT(
            emrtd_pace_run(emrtd_sim_transceiver(chip), &info, &credentials, &session),
            EmrtdErrorNone);
        TEST_EQ_INT(session.cipher, ciphers[i]);
        emrtd_sm_clear(&session);
        emrtd_sim_free(chip);
    }

    emrtd_test_begin("NIST P-256 works as well as the brainpool curve");
    EmrtdSim* const chip = pace_chip(EmrtdCipherAes128, 12);
    TEST_CHECK(chip != NULL);

    EmrtdPaceInfo info;
    TEST_CHECK(read_pace_info(chip, &info));
    TEST_EQ_STR(info.curve->name, "NIST P-256");

    EmrtdSm session;
    TEST_EQ_INT(
        emrtd_pace_run(emrtd_sim_transceiver(chip), &info, &credentials, &session),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);
}

static void test_wrong_password(void) {
    emrtd_test_begin("a password that is not this document's is reported as such");

    EmrtdSim* const chip = pace_chip(EmrtdCipherAes128, 13);
    TEST_CHECK(chip != NULL);

    EmrtdPaceInfo info;
    TEST_CHECK(read_pace_info(chip, &info));

    /*
     * A wrong MRZ decrypts the nonce to something else, so the mapped
     * generators differ and neither token can verify. The chip notices first
     * and answers 63CX, counting down the attempts it will allow.
     */
    const EmrtdCredentials wrong = credentials_for("Z00000000", "010101", "010101");
    EmrtdSm session;
    TEST_EQ_INT(
        emrtd_pace_run(emrtd_sim_transceiver(chip), &info, &wrong, &session), EmrtdErrorWrongKey);
    TEST_CHECK(!session.established);
    TEST_CHECK(!emrtd_sim_authenticated(chip));

    emrtd_sim_free(chip);

    emrtd_test_begin("a chip that will not start the protocol is reported, not retried");
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessBacOnly;
    config.credentials = credentials_for("L898902C", "690806", "300701");
    EmrtdSim* const bac_only = emrtd_sim_alloc(&config);
    TEST_CHECK(bac_only != NULL);

    /* The PACEInfo is invented here, because a BAC-only chip publishes none. */
    memset(&info, 0, sizeof(info));
    emrtd_test_hex(PACE_OID_AES128, info.oid, sizeof(info.oid));
    info.oid_len = 10;
    info.cipher = EmrtdCipherAes128;
    info.curve = emrtd_ec_curve_by_param_id(13);
    info.mapping = EmrtdPaceMappingGeneric;
    info.agreement = EmrtdPaceAgreementEcdh;
    info.usable = true;

    const EmrtdCredentials credentials = credentials_for("L898902C", "690806", "300701");
    /*
     * The exact error is whatever the chip's status word maps to; what matters
     * here is that MSE:Set AT being refused ends the attempt at once, so the
     * reader can fall back to the other driver instead of sending three more
     * rounds into a chip that has already said no.
     */
    TEST_CHECK(
        emrtd_pace_run(emrtd_sim_transceiver(bac_only), &info, &credentials, &session) !=
        EmrtdErrorNone);
    TEST_CHECK(!session.established);
    TEST_EQ_INT(emrtd_sim_exchanges(bac_only), 1);

    emrtd_sim_free(bac_only);
}

void test_suite_pace(void) {
    test_password_key();
    test_token();
    test_refusals_before_the_first_apdu();
    test_exchange();
    test_other_ciphers_and_curves();
    test_wrong_password();
}
