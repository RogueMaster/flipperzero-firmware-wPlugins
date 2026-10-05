/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * A whole read, from an unopened chip to the bytes of a data group.
 *
 * Every other suite checks one layer against a vector. This one checks that
 * the layers fit together: a driver opens a session, the session protects the
 * commands that follow, and the frame arithmetic keeps each answer inside one
 * ISO 14443-4 frame - because the Flipper's stack does not reassemble a
 * chained response, and a reader that asks for one byte too many gets nothing
 * at all. The simulated chip refuses to answer over that limit, so an
 * arithmetic mistake fails here rather than against a passport.
 */

#include "emrtd_test.h"

#include "../../access/emrtd_access.h"
#include "../../protocol/emrtd_apdu.h"
#include "../../protocol/emrtd_lds.h"
#include "../../transport/emrtd_transceiver.h"
#include "../../sim/emrtd_sim.h"

#include <mbedtls/sha256.h>

#include <mbedtls/sha256.h>

static EmrtdCredentials specimen(void) {
    EmrtdCredentials credentials;
    memset(&credentials, 0, sizeof(credentials));
    strcpy(credentials.document_number, "L898902C");
    strcpy(credentials.date_of_birth, "690806");
    strcpy(credentials.date_of_expiry, "300701");
    return credentials;
}

/* --- Frame arithmetic ---------------------------------------------------- */

static void test_frame_sizes(void) {
    emrtd_test_begin("the ATS format byte decodes to a frame size (ISO 14443-4 table 3)");

    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(0), 16);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(1), 24);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(2), 32);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(5), 64);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(7), 128);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(8), 256);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(12), 4096);
    /* Codes 13 to 15 are reserved; fall back on what this reader can handle. */
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(13), EMRTD_APDU_MAX_SIZE);
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(15), EMRTD_APDU_MAX_SIZE);
    /* Only the low nibble of T0 carries FSCI. */
    TEST_EQ_INT(emrtd_transceiver_fsc_from_fsci(0x78), 256);

    emrtd_test_begin("the expected length leaves room for the envelope");
    EmrtdTransceiver transceiver = {.api = NULL, .ctx = NULL, .fsc = 256, .fsd = 256};

    /* With no session the answer is plain, so only the status word is reserved. */
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 0), 250);
    /*
     * Under AES the answer carries DO'87' with its header and padding
     * indicator, DO'99', DO'8E' and the status word, and the plaintext is
     * rounded down to a whole block with one block held back, because method 2
     * padding is added even to a plaintext that already fits.
     */
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 16), 208);
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 8), 224);

    emrtd_test_begin("a small frame yields a small read, never a huge one");
    /*
     * Sixty four bytes of frame leave forty for the plaintext once the
     * envelope and the status word are paid for, and a thirty two byte
     * plaintext would pad to forty eight and overflow it. Sixteen is what
     * fits, and asking for more is exactly the mistake that produces a chained
     * response the Flipper cannot reassemble.
     */
    transceiver.fsd = 64;
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 16), 16);
    /*
     * The ISO default frame of 32 bytes cannot hold an envelope at all. The
     * answer has to be zero rather than the enormous number an unguarded
     * subtraction would produce.
     */
    transceiver.fsd = 32;
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 16), 0);
    transceiver.fsd = 16;
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 16), 0);
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 0), 10);
    TEST_EQ_INT(emrtd_transceiver_max_le(NULL, 16), 0);

    emrtd_test_begin("the command length follows the card's frame, not the reader's");
    transceiver.fsc = 256;
    transceiver.fsd = 16;
    TEST_EQ_INT(emrtd_transceiver_max_lc(&transceiver), 246);
    transceiver.fsc = 64;
    TEST_EQ_INT(emrtd_transceiver_max_lc(&transceiver), 54);
    transceiver.fsc = 16;
    TEST_EQ_INT(emrtd_transceiver_max_lc(&transceiver), 6);
    /* A frame too small for a header at all gives nothing, not an underflow. */
    transceiver.fsc = 8;
    TEST_EQ_INT(emrtd_transceiver_max_lc(&transceiver), 0);
    TEST_EQ_INT(emrtd_transceiver_max_lc(NULL), 0);

    emrtd_test_begin("a frame size of zero means the ISO default of 32");
    transceiver.fsc = 0;
    transceiver.fsd = 0;
    TEST_EQ_INT(emrtd_transceiver_max_lc(&transceiver), 22);
    TEST_EQ_INT(emrtd_transceiver_max_le(&transceiver, 0), 26);
}

static void test_exchange_guards(void) {
    emrtd_test_begin("the port refuses what it cannot pass on");

    EmrtdSim* const chip = emrtd_sim_alloc(NULL);
    EmrtdTransceiver* const transceiver = emrtd_sim_transceiver(chip);
    uint8_t command[8] = {0x00, 0xA4, 0x04, 0x0C};
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    size_t response_len = 0;

    TEST_EQ_INT(
        emrtd_transceiver_exchange(NULL, command, 4, response, sizeof(response), &response_len),
        EmrtdErrorInternal);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(
            transceiver, command, 3, response, sizeof(response), &response_len),
        EmrtdErrorInternal);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(transceiver, command, 4, response, 1, &response_len),
        EmrtdErrorBufferTooSmall);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(transceiver, NULL, 4, response, sizeof(response), &response_len),
        EmrtdErrorInternal);

    emrtd_sim_free(chip);
}

/* --- A read, end to end -------------------------------------------------- */

/** SELECT an elementary file inside the open session. */
static EmrtdError select_file(EmrtdTransceiver* transceiver, EmrtdSm* session, uint16_t fid) {
    uint8_t identifier[2] = {(uint8_t)(fid >> 8), (uint8_t)(fid & 0xFF)};
    EmrtdCommandApdu command;
    emrtd_apdu_select_file(&command, identifier);

    uint8_t buffer[EMRTD_APDU_MAX_SIZE];
    size_t len = 0;
    EmrtdError error = emrtd_sm_protect(session, &command, buffer, sizeof(buffer), &len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    size_t response_len = 0;
    error = emrtd_transceiver_exchange(
        transceiver, buffer, len, buffer, sizeof(buffer), &response_len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    EmrtdResponseApdu parsed;
    error = emrtd_sm_unprotect(session, buffer, response_len, buffer, sizeof(buffer), &parsed);
    if(error != EmrtdErrorNone) {
        return error;
    }
    return parsed.sw == 0x9000 ? EmrtdErrorNone : emrtd_error_from_sw(parsed.sw);
}

/**
 * Read the selected file to its end, one frame at a time.
 *
 * This is the loop the worker performs, reduced to what a test needs: ask for
 * as much as a frame will hold, stop when the chip says there is no more.
 */
static EmrtdError read_selected(
    EmrtdTransceiver* transceiver,
    EmrtdSm* session,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    const size_t chunk =
        emrtd_transceiver_max_le(transceiver, emrtd_cipher_block_size(session->cipher));
    if(chunk == 0) {
        return EmrtdErrorBufferTooSmall;
    }

    *out_len = 0;
    while(*out_len < out_size) {
        EmrtdCommandApdu command;
        emrtd_apdu_read_binary(&command, (uint16_t)*out_len, chunk);

        uint8_t buffer[EMRTD_APDU_MAX_SIZE];
        size_t len = 0;
        EmrtdError error = emrtd_sm_protect(session, &command, buffer, sizeof(buffer), &len);
        if(error != EmrtdErrorNone) {
            return error;
        }

        size_t response_len = 0;
        error = emrtd_transceiver_exchange(
            transceiver, buffer, len, buffer, sizeof(buffer), &response_len);
        if(error != EmrtdErrorNone) {
            return error;
        }

        EmrtdResponseApdu parsed;
        error = emrtd_sm_unprotect(session, buffer, response_len, buffer, sizeof(buffer), &parsed);
        if(error != EmrtdErrorNone) {
            return error;
        }
        if(parsed.sw == 0x6B00) {
            /* Past the end of the file: everything has been read. */
            return EmrtdErrorNone;
        }
        if(parsed.sw != 0x9000) {
            return emrtd_error_from_sw(parsed.sw);
        }
        if(parsed.data_len == 0 || parsed.data_len > out_size - *out_len) {
            return EmrtdErrorProtocol;
        }
        memcpy(out + *out_len, parsed.data, parsed.data_len);
        *out_len += parsed.data_len;

        if(parsed.data_len < chunk) {
            /* A short answer is the last one. */
            return EmrtdErrorNone;
        }
    }
    return EmrtdErrorNone;
}

/** Select the eMRTD application, which after PACE happens inside the session. */
static EmrtdError select_application(EmrtdTransceiver* transceiver, EmrtdSm* session) {
    EmrtdCommandApdu command;
    emrtd_apdu_select_application(&command, EMRTD_AID, sizeof(EMRTD_AID));

    uint8_t buffer[EMRTD_APDU_MAX_SIZE];
    size_t len = 0;
    EmrtdError error = emrtd_sm_protect(session, &command, buffer, sizeof(buffer), &len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    size_t response_len = 0;
    error = emrtd_transceiver_exchange(
        transceiver, buffer, len, buffer, sizeof(buffer), &response_len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    EmrtdResponseApdu parsed;
    error = emrtd_sm_unprotect(session, buffer, response_len, buffer, sizeof(buffer), &parsed);
    if(error != EmrtdErrorNone) {
        return error;
    }
    return parsed.sw == 0x9000 ? EmrtdErrorNone : emrtd_error_from_sw(parsed.sw);
}

/** Check that one file reads back exactly as the chip holds it. */
static void check_file(EmrtdSim* chip, EmrtdSm* session, uint16_t fid) {
    size_t expected_len = 0;
    const uint8_t* const expected = emrtd_sim_file(chip, fid, &expected_len);
    TEST_CHECK(expected != NULL);

    TEST_EQ_INT(select_file(emrtd_sim_transceiver(chip), session, fid), EmrtdErrorNone);

    /* Large enough for EF.SOD, which is the one file here that needs more
     * than a handful of frames. */
    uint8_t data[2048];
    size_t len = 0;
    TEST_EQ_INT(
        read_selected(emrtd_sim_transceiver(chip), session, data, sizeof(data), &len),
        EmrtdErrorNone);
    TEST_EQ_INT(len, expected_len);
    TEST_CHECK(len == expected_len && memcmp(data, expected, len) == 0);
}

/** Open a chip with the driver that suits it, then read its files. */
static void read_document(EmrtdSimAccess access, const char* expected_protocol, uint16_t frame) {
    const EmrtdCredentials credentials = specimen();
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = access;
    config.credentials = credentials;
    config.fsc = frame;
    config.fsd = frame;

    EmrtdSim* const chip = emrtd_sim_alloc(&config);
    TEST_CHECK(chip != NULL);
    EmrtdTransceiver* const transceiver = emrtd_sim_transceiver(chip);

    /* The application is selected in the clear, before anything is protected. */
    uint8_t command[EMRTD_APDU_MAX_SIZE];
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    size_t command_len = 0;
    size_t response_len = 0;
    EmrtdCommandApdu apdu;
    EmrtdResponseApdu parsed;
    emrtd_apdu_select_application(&apdu, EMRTD_AID, sizeof(EMRTD_AID));
    TEST_EQ_INT(emrtd_apdu_encode(&apdu, command, sizeof(command), &command_len), EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(
            transceiver, command, command_len, response, sizeof(response), &response_len),
        EmrtdErrorNone);
    TEST_EQ_INT(emrtd_apdu_decode(response, response_len, &parsed), EmrtdErrorNone);
    TEST_EQ_INT(parsed.sw, 0x9000);

    /* EF.CardAccess is readable before any session exists, when there is one. */
    uint8_t card_access[64];
    size_t card_access_len = 0;
    uint8_t identifier[2] = {0x01, 0x1C};
    emrtd_apdu_select_file(&apdu, identifier);
    TEST_EQ_INT(emrtd_apdu_encode(&apdu, command, sizeof(command), &command_len), EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_transceiver_exchange(
            transceiver, command, command_len, response, sizeof(response), &response_len),
        EmrtdErrorNone);
    TEST_EQ_INT(emrtd_apdu_decode(response, response_len, &parsed), EmrtdErrorNone);

    if(parsed.sw == 0x9000) {
        emrtd_apdu_read_binary(&apdu, 0, emrtd_transceiver_max_le(transceiver, 0));
        TEST_EQ_INT(
            emrtd_apdu_encode(&apdu, command, sizeof(command), &command_len), EmrtdErrorNone);
        TEST_EQ_INT(
            emrtd_transceiver_exchange(
                transceiver, command, command_len, response, sizeof(response), &response_len),
            EmrtdErrorNone);
        TEST_EQ_INT(emrtd_apdu_decode(response, response_len, &parsed), EmrtdErrorNone);
        if(parsed.sw == 0x9000 && parsed.data_len <= sizeof(card_access)) {
            memcpy(card_access, parsed.data, parsed.data_len);
            card_access_len = parsed.data_len;
        }
    }

    /* Let the registry choose, exactly as the reader does. */
    const EmrtdAccessDriver* chosen = NULL;
    EmrtdAccessScore best = EmrtdAccessScoreUnsupported;
    for(size_t i = 0; i < emrtd_access_driver_count(); i++) {
        const EmrtdAccessDriver* const driver = emrtd_access_driver_at(i);
        EmrtdError reason = EmrtdErrorNone;
        const EmrtdAccessScore score = driver->probe(
            card_access_len != 0 ? card_access : NULL, card_access_len, &credentials, &reason);
        if(score > best) {
            best = score;
            chosen = driver;
        }
    }
    TEST_CHECK(chosen != NULL);

    EmrtdSm session;
    EmrtdAccessOutcome outcome;
    TEST_EQ_INT(
        chosen->authenticate(
            transceiver,
            card_access_len != 0 ? card_access : NULL,
            card_access_len,
            &credentials,
            &session,
            &outcome),
        EmrtdErrorNone);
    TEST_EQ_STR(outcome.protocol, expected_protocol);
    TEST_EQ_STR(emrtd_sim_protocol(chip), expected_protocol);

    if(chosen->reselect_application) {
        TEST_EQ_INT(select_application(transceiver, &session), EmrtdErrorNone);
    }

    check_file(chip, &session, EMRTD_SIM_FID_COM);
    check_file(chip, &session, EMRTD_SIM_FID_DG1);
    check_file(chip, &session, EMRTD_SIM_FID_SOD);

    emrtd_test_begin("a file the chip does not hold is reported as missing");
    TEST_EQ_INT(select_file(transceiver, &session, 0x0102), EmrtdErrorFileNotFound);

    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);
}

static void test_read_over_pace(void) {
    emrtd_test_begin("a PACE document is opened and read to the last byte");
    read_document(EmrtdSimAccessBoth, "PACE", 256);
}

static void test_read_over_bac(void) {
    emrtd_test_begin("a document with no EF.CardAccess falls back to BAC and reads");
    read_document(EmrtdSimAccessBacOnly, "BAC", 256);
}

static void test_read_in_small_frames(void) {
    emrtd_test_begin("the same read works when every answer has to fit 96 bytes");
    /*
     * A frame this small forces several rounds per file, which is what a real
     * document needs for DG2. The chip refuses to answer beyond the frame, so
     * an expected length computed one byte too generously fails here.
     */
    read_document(EmrtdSimAccessBoth, "PACE", 96);
    read_document(EmrtdSimAccessBacOnly, "BAC", 96);
}

static void test_session_ends_with_the_read(void) {
    emrtd_test_begin("a chip that has lost the session says so rather than answering");

    const EmrtdCredentials credentials = specimen();
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessBacOnly;
    config.credentials = credentials;
    EmrtdSim* const chip = emrtd_sim_alloc(&config);
    EmrtdTransceiver* const transceiver = emrtd_sim_transceiver(chip);

    EmrtdSm session;
    EmrtdAccessOutcome outcome;
    TEST_EQ_INT(
        emrtd_access_driver_bac.authenticate(
            transceiver, NULL, 0, &credentials, &session, &outcome),
        EmrtdErrorNone);

    /*
     * Once the counters have drifted apart nothing can be recovered, and the
     * reader has to say so instead of handing rubbish to the parser. Skipping
     * a command on our side is the cheapest way to produce that state.
     */
    uint8_t scratch[EMRTD_APDU_MAX_SIZE];
    size_t len = 0;
    EmrtdCommandApdu command;
    uint8_t identifier[2] = {0x01, 0x1E};
    emrtd_apdu_select_file(&command, identifier);
    TEST_EQ_INT(
        emrtd_sm_protect(&session, &command, scratch, sizeof(scratch), &len), EmrtdErrorNone);
    /* The command is built and then thrown away, so the chip never sees it. */
    TEST_EQ_INT(select_file(transceiver, &session, EMRTD_SIM_FID_COM), EmrtdErrorSecureMessaging);

    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);
}

/**
 * The security object the chip serves has to describe the chip.
 *
 * EF.SOD is the one file the simulator does not simply make up: it carries a
 * real LDSSecurityObject whose DG1 entry is rewritten with the hash of the
 * DG1 that particular chip serves, and DG1 is built from the credentials it
 * was given. Get that patch wrong - the wrong offset, the wrong bytes hashed,
 * the wrong digest - and nothing fails until a reader compares the two and
 * reports a document that has been tampered with. So it is compared here.
 */
static void test_security_object_describes_the_chip(void) {
    emrtd_test_begin("the security object lists the hash of the DG1 this chip serves");

    /* Deliberately not the default credentials, so that a frozen hash which
     * happened to suit the specimen would still be caught. */
    EmrtdCredentials credentials;
    memset(&credentials, 0, sizeof(credentials));
    strcpy(credentials.document_number, "T220001293");
    strcpy(credentials.date_of_birth, "640812");
    strcpy(credentials.date_of_expiry, "101031");

    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessBoth;
    config.credentials = credentials;
    EmrtdSim* const chip = emrtd_sim_alloc(&config);
    TEST_CHECK(chip != NULL);

    size_t sod_len = 0;
    const uint8_t* const sod_bytes = emrtd_sim_file(chip, EMRTD_SIM_FID_SOD, &sod_len);
    TEST_CHECK(sod_bytes != NULL);
    TEST_EQ_INT(sod_len, 1426);

    EmrtdEfSod sod;
    TEST_EQ_INT(emrtd_lds_parse_sod(sod_bytes, sod_len, &sod), EmrtdErrorNone);
    TEST_EQ_STR(sod.digest_algorithm, "SHA-256");
    TEST_CHECK(sod.digest_supported);
    TEST_EQ_INT(sod.digest_len, 32);
    TEST_CHECK(sod.has_certificate);

    size_t dg1_len = 0;
    const uint8_t* const dg1 = emrtd_sim_file(chip, EMRTD_SIM_FID_DG1, &dg1_len);
    TEST_CHECK(dg1 != NULL);

    uint8_t digest[32];
    TEST_EQ_INT(mbedtls_sha256(dg1, dg1_len, digest, 0), 0);

    const EmrtdSodHash* const listed = emrtd_lds_sod_hash_for(&sod, 1);
    TEST_CHECK(listed != NULL);
    if(listed != NULL) {
        TEST_EQ_INT(listed->hash_len, sizeof(digest));
        TEST_CHECK(memcmp(listed->hash, digest, sizeof(digest)) == 0);
    }

    emrtd_sim_free(chip);
}

void test_suite_session(void) {
    test_frame_sizes();
    test_exchange_guards();
    test_read_over_pace();
    test_read_over_bac();
    test_read_in_small_frames();
    test_session_ends_with_the_read();
    test_security_object_describes_the_chip();
}
