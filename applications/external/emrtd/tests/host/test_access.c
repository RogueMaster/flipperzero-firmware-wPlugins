/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The access driver registry.
 *
 * Which protocol opens a passport is not something the person holding it
 * knows, so the reader asks each driver what it makes of the chip and runs the
 * most promising one. What is checked here is that the verdicts are the ones
 * that make that choice come out right: a chip that advertises PACE outranks
 * the guess that BAC will work, an unusable PACEInfo is refused by name rather
 * than attempted, and credentials that cannot derive a key stop a driver
 * before it touches the field.
 */

#include "emrtd_test.h"

#include "../../access/emrtd_access.h"
#include "../../sim/emrtd_sim.h"

static EmrtdCredentials specimen(void) {
    EmrtdCredentials credentials;
    memset(&credentials, 0, sizeof(credentials));
    strcpy(credentials.document_number, "L898902C");
    strcpy(credentials.date_of_birth, "690806");
    strcpy(credentials.date_of_expiry, "300701");
    return credentials;
}

static void test_registry(void) {
    emrtd_test_begin("the registry lists PACE ahead of BAC");

    TEST_EQ_INT(emrtd_access_driver_count(), 2);
    TEST_CHECK(emrtd_access_driver_at(0) == &emrtd_access_driver_pace);
    TEST_CHECK(emrtd_access_driver_at(1) == &emrtd_access_driver_bac);
    TEST_CHECK(emrtd_access_driver_at(2) == NULL);
    TEST_EQ_STR(emrtd_access_driver_at(0)->name, "PACE");
    TEST_EQ_STR(emrtd_access_driver_at(1)->name, "BAC");

    emrtd_test_begin("a fixed choice maps to one driver, automatic to none");
    TEST_CHECK(emrtd_access_driver_for_method(EmrtdAccessMethodPace) == &emrtd_access_driver_pace);
    TEST_CHECK(emrtd_access_driver_for_method(EmrtdAccessMethodBac) == &emrtd_access_driver_bac);
    TEST_CHECK(emrtd_access_driver_for_method(EmrtdAccessMethodAuto) == NULL);
    TEST_EQ_STR(emrtd_access_method_name(EmrtdAccessMethodAuto), "Automatic");
    TEST_EQ_STR(emrtd_access_method_name(EmrtdAccessMethodPace), "PACE");
    TEST_EQ_STR(emrtd_access_method_name(EmrtdAccessMethodBac), "BAC");

    emrtd_test_begin("PACE has to select the application again, BAC does not");
    /*
     * PACE runs against the master file, where EF.CardAccess lives, so the
     * eMRTD application is no longer selected once the session exists.
     */
    TEST_CHECK(emrtd_access_driver_pace.reselect_application);
    TEST_CHECK(!emrtd_access_driver_bac.reselect_application);
}

static void test_credentials(void) {
    emrtd_test_begin("credentials are checked before they are hashed into a key");

    EmrtdCredentials credentials = specimen();
    TEST_CHECK(emrtd_credentials_valid_mrz(&credentials));
    TEST_CHECK(!emrtd_credentials_valid_mrz(NULL));

    /* A document number the check digit arithmetic cannot score. */
    EmrtdCredentials invalid = credentials;
    strcpy(invalid.document_number, "L8989 2C");
    TEST_CHECK(!emrtd_credentials_valid_mrz(&invalid));

    invalid = credentials;
    invalid.document_number[0] = '\0';
    TEST_CHECK(!emrtd_credentials_valid_mrz(&invalid));

    /* Dates are six digits, always; a short one would shift every field after it. */
    invalid = credentials;
    strcpy(invalid.date_of_birth, "69080");
    TEST_CHECK(!emrtd_credentials_valid_mrz(&invalid));

    invalid = credentials;
    strcpy(invalid.date_of_expiry, "30070A");
    TEST_CHECK(!emrtd_credentials_valid_mrz(&invalid));

    /* The filler character is legal, since a short number is padded with it. */
    invalid = credentials;
    strcpy(invalid.document_number, "L898902C<");
    TEST_CHECK(emrtd_credentials_valid_mrz(&invalid));
}

static void test_probes(void) {
    emrtd_test_begin("a chip that advertises PACE outranks the assumption of BAC");

    const EmrtdCredentials credentials = specimen();
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessBoth;
    config.credentials = credentials;
    EmrtdSim* const chip = emrtd_sim_alloc(&config);
    TEST_CHECK(chip != NULL);

    size_t card_access_len = 0;
    const uint8_t* const card_access =
        emrtd_sim_file(chip, EMRTD_SIM_FID_CARD_ACCESS, &card_access_len);
    TEST_CHECK(card_access != NULL);

    EmrtdError reason = EmrtdErrorNone;
    TEST_EQ_INT(
        emrtd_access_driver_pace.probe(card_access, card_access_len, &credentials, &reason),
        EmrtdAccessScoreAnnounced);
    TEST_EQ_INT(
        emrtd_access_driver_bac.probe(card_access, card_access_len, &credentials, &reason),
        EmrtdAccessScorePossible);

    emrtd_test_begin("without EF.CardAccess there is no PACE to run");
    /*
     * The object identifier and the domain parameters only appear in that
     * file, so a chip that does not publish it cannot be opened with PACE
     * however well the reader guesses.
     */
    reason = EmrtdErrorNone;
    TEST_EQ_INT(
        emrtd_access_driver_pace.probe(NULL, 0, &credentials, &reason),
        EmrtdAccessScoreUnsupported);
    TEST_EQ_INT(reason, EmrtdErrorNoAccessMethod);
    TEST_EQ_INT(
        emrtd_access_driver_bac.probe(NULL, 0, &credentials, &reason), EmrtdAccessScorePossible);

    emrtd_test_begin("a curve this build cannot compute is named, not attempted");
    EmrtdSimConfig big;
    memset(&big, 0, sizeof(big));
    big.access = EmrtdSimAccessBoth;
    big.credentials = credentials;
    /* Id 17 is brainpoolP512r1, above MBEDTLS_ECP_MAX_BITS; see docs/platform.md. */
    big.pace_parameter_id = 17;
    EmrtdSim* const big_chip = emrtd_sim_alloc(&big);
    TEST_CHECK(big_chip != NULL);

    size_t big_len = 0;
    const uint8_t* const big_access =
        emrtd_sim_file(big_chip, EMRTD_SIM_FID_CARD_ACCESS, &big_len);
    reason = EmrtdErrorNone;
    TEST_EQ_INT(
        emrtd_access_driver_pace.probe(big_access, big_len, &credentials, &reason),
        EmrtdAccessScoreUnsupported);
    TEST_EQ_INT(reason, EmrtdErrorPaceUnsupportedCurve);
    /* BAC is still worth trying on that document. */
    TEST_EQ_INT(
        emrtd_access_driver_bac.probe(big_access, big_len, &credentials, &reason),
        EmrtdAccessScorePossible);
    emrtd_sim_free(big_chip);

    emrtd_test_begin("credentials that open nothing stop both drivers");
    EmrtdCredentials empty;
    memset(&empty, 0, sizeof(empty));
    reason = EmrtdErrorNone;
    TEST_EQ_INT(
        emrtd_access_driver_pace.probe(card_access, card_access_len, &empty, &reason),
        EmrtdAccessScoreUnsupported);
    TEST_EQ_INT(reason, EmrtdErrorInvalidInput);
    reason = EmrtdErrorNone;
    TEST_EQ_INT(
        emrtd_access_driver_bac.probe(card_access, card_access_len, &empty, &reason),
        EmrtdAccessScoreUnsupported);
    TEST_EQ_INT(reason, EmrtdErrorInvalidInput);

    emrtd_test_begin("a Card Access Number is password enough for PACE alone");
    EmrtdCredentials can_only;
    memset(&can_only, 0, sizeof(can_only));
    can_only.has_can = true;
    strcpy(can_only.can, "123456");
    TEST_EQ_INT(
        emrtd_access_driver_pace.probe(card_access, card_access_len, &can_only, &reason),
        EmrtdAccessScoreAnnounced);
    /* BAC has no use for a CAN, so it must not claim the document. */
    TEST_EQ_INT(
        emrtd_access_driver_bac.probe(card_access, card_access_len, &can_only, &reason),
        EmrtdAccessScoreUnsupported);

    emrtd_sim_free(chip);
}

/**
 * The selection the reader performs for EmrtdAccessMethodAuto.
 *
 * The worker owns this policy in the application; it is written out here so
 * that the registry can be shown to support it - drivers in preference order,
 * the highest score first, ties going to the earlier entry.
 */
static const EmrtdAccessDriver* best_driver(
    const uint8_t* card_access,
    size_t card_access_len,
    const EmrtdCredentials* credentials) {
    const EmrtdAccessDriver* best = NULL;
    EmrtdAccessScore best_score = EmrtdAccessScoreUnsupported;

    for(size_t i = 0; i < emrtd_access_driver_count(); i++) {
        const EmrtdAccessDriver* const driver = emrtd_access_driver_at(i);
        EmrtdError reason = EmrtdErrorNone;
        const EmrtdAccessScore score =
            driver->probe(card_access, card_access_len, credentials, &reason);
        if(score > best_score) {
            best_score = score;
            best = driver;
        }
    }
    return best;
}

static void test_selection_and_fallback(void) {
    emrtd_test_begin("a chip offering both is opened with PACE");

    const EmrtdCredentials credentials = specimen();
    EmrtdSimConfig config;
    memset(&config, 0, sizeof(config));
    config.access = EmrtdSimAccessBoth;
    config.credentials = credentials;
    EmrtdSim* chip = emrtd_sim_alloc(&config);

    size_t card_access_len = 0;
    const uint8_t* card_access = emrtd_sim_file(chip, EMRTD_SIM_FID_CARD_ACCESS, &card_access_len);
    const EmrtdAccessDriver* driver = best_driver(card_access, card_access_len, &credentials);
    TEST_CHECK(driver == &emrtd_access_driver_pace);

    EmrtdSm session;
    EmrtdAccessOutcome outcome;
    TEST_EQ_INT(
        driver->authenticate(
            emrtd_sim_transceiver(chip),
            card_access,
            card_access_len,
            &credentials,
            &session,
            &outcome),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    TEST_EQ_STR(outcome.protocol, "PACE");
    TEST_EQ_STR(outcome.summary, "PACE ECDH-GM/AES-128, brainpoolP256r1");
    TEST_EQ_STR(emrtd_sim_protocol(chip), "PACE");
    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);

    emrtd_test_begin("a chip that publishes nothing is opened with BAC");
    config.access = EmrtdSimAccessBacOnly;
    chip = emrtd_sim_alloc(&config);
    card_access = emrtd_sim_file(chip, EMRTD_SIM_FID_CARD_ACCESS, &card_access_len);
    TEST_CHECK(card_access == NULL);

    driver = best_driver(NULL, 0, &credentials);
    TEST_CHECK(driver == &emrtd_access_driver_bac);
    TEST_EQ_INT(
        driver->authenticate(
            emrtd_sim_transceiver(chip), NULL, 0, &credentials, &session, &outcome),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    TEST_EQ_INT(session.cipher, EmrtdCipherTdes);
    TEST_EQ_STR(outcome.protocol, "BAC");
    TEST_EQ_STR(emrtd_sim_protocol(chip), "BAC");
    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);

    emrtd_test_begin("the second driver still runs after the first has failed");
    /*
     * This is the fallback the registry exists for: PACE is advertised and
     * attempted, the password turns out to be wrong for it, and BAC is tried
     * on the same chip afterwards. The chip has to be met again from the
     * start, which is what the reader does when it re-selects the application.
     */
    config.access = EmrtdSimAccessBoth;
    chip = emrtd_sim_alloc(&config);
    card_access = emrtd_sim_file(chip, EMRTD_SIM_FID_CARD_ACCESS, &card_access_len);

    EmrtdCredentials wrong = specimen();
    strcpy(wrong.document_number, "Z00000000");
    TEST_EQ_INT(
        emrtd_access_driver_pace.authenticate(
            emrtd_sim_transceiver(chip), card_access, card_access_len, &wrong, &session, &outcome),
        EmrtdErrorWrongKey);
    TEST_CHECK(!session.established);
    TEST_CHECK(!emrtd_sim_authenticated(chip));

    TEST_EQ_INT(
        emrtd_access_driver_bac.authenticate(
            emrtd_sim_transceiver(chip),
            card_access,
            card_access_len,
            &credentials,
            &session,
            &outcome),
        EmrtdErrorNone);
    TEST_CHECK(session.established);
    TEST_EQ_STR(outcome.protocol, "BAC");
    emrtd_sm_clear(&session);
    emrtd_sim_free(chip);

    emrtd_test_begin("a wrong MRZ is reported as such by BAC, not as a broken chip");
    config.access = EmrtdSimAccessBacOnly;
    chip = emrtd_sim_alloc(&config);
    TEST_EQ_INT(
        emrtd_access_driver_bac.authenticate(
            emrtd_sim_transceiver(chip), NULL, 0, &wrong, &session, &outcome),
        EmrtdErrorWrongKey);
    TEST_CHECK(!session.established);
    emrtd_sim_free(chip);

    emrtd_test_begin("a driver given nothing to work with refuses cleanly");
    TEST_EQ_INT(
        emrtd_access_driver_bac.authenticate(NULL, NULL, 0, &credentials, &session, &outcome),
        EmrtdErrorInternal);
    TEST_EQ_INT(
        emrtd_access_driver_pace.authenticate(NULL, NULL, 0, &credentials, &session, &outcome),
        EmrtdErrorInternal);
}

void test_suite_access(void) {
    test_registry();
    test_credentials();
    test_probes();
    test_selection_and_fallback();
}
