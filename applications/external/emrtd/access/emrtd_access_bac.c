/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include <stdio.h>
#include <string.h>

#include "../crypto/emrtd_bac.h"
#include "../crypto/emrtd_rng.h"
#include "../protocol/emrtd_apdu.h"
#include "emrtd_access.h"

#include "../emrtd_wipe.h"

/** RND.IC, the challenge the chip answers GET CHALLENGE with. */
#define EMRTD_BAC_CHALLENGE_SIZE 8
/** E.IFD || M.IFD, and the answer E.IC || M.IC. */
#define EMRTD_BAC_PAYLOAD_SIZE   40

static EmrtdAccessScore emrtd_access_bac_probe(
    const uint8_t* card_access,
    size_t card_access_len,
    const EmrtdCredentials* credentials,
    EmrtdError* reason) {
    /*
     * EF.CardAccess says nothing about BAC: it lists the protocols a chip
     * announces, and BAC is the one that is simply assumed to be there. So the
     * verdict rests entirely on whether the MRZ input can derive a key, which
     * makes BAC "possible" rather than "announced" and puts it behind PACE
     * whenever a chip advertises that.
     */
    (void)card_access;
    (void)card_access_len;

    if(!emrtd_credentials_valid_mrz(credentials)) {
        if(reason != NULL) {
            *reason = EmrtdErrorInvalidInput;
        }
        return EmrtdAccessScoreUnsupported;
    }
    return EmrtdAccessScorePossible;
}

static EmrtdError emrtd_access_bac_authenticate(
    EmrtdTransceiver* transceiver,
    const uint8_t* card_access,
    size_t card_access_len,
    const EmrtdCredentials* credentials,
    EmrtdSm* out_session,
    EmrtdAccessOutcome* out_outcome) {
    (void)card_access;
    (void)card_access_len;

    if(transceiver == NULL || credentials == NULL || out_session == NULL) {
        return EmrtdErrorInternal;
    }
    if(!emrtd_credentials_valid_mrz(credentials)) {
        return EmrtdErrorInvalidInput;
    }

    uint8_t k_enc[16];
    uint8_t k_mac[16];
    EmrtdError error = emrtd_bac_derive_keys(credentials, k_enc, k_mac);
    if(error != EmrtdErrorNone) {
        return error;
    }

    uint8_t command[EMRTD_APDU_MAX_SIZE];
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    uint8_t rnd_ic[EMRTD_BAC_CHALLENGE_SIZE];
    uint8_t rnd_ifd[8];
    uint8_t k_ifd[16];
    uint8_t payload[EMRTD_BAC_PAYLOAD_SIZE];
    size_t command_len = 0;
    size_t response_len = 0;
    EmrtdCommandApdu apdu;
    EmrtdResponseApdu parsed;

    /* Step 1: ask the chip for its challenge (9303-11, 4.3.3). */
    emrtd_apdu_get_challenge(&apdu);
    error = emrtd_apdu_encode(&apdu, command, sizeof(command), &command_len);
    if(error == EmrtdErrorNone) {
        error = emrtd_transceiver_exchange(
            transceiver, command, command_len, response, sizeof(response), &response_len);
    }
    if(error == EmrtdErrorNone) {
        error = emrtd_apdu_decode(response, response_len, &parsed);
    }
    if(error == EmrtdErrorNone && !emrtd_apdu_is_success(&parsed)) {
        error = emrtd_error_from_sw(parsed.sw);
    }
    if(error == EmrtdErrorNone && parsed.data_len != sizeof(rnd_ic)) {
        /* A chip that cannot produce eight bytes of challenge is not doing BAC. */
        error = EmrtdErrorProtocol;
    }
    if(error != EmrtdErrorNone) {
        goto cleanup;
    }
    memcpy(rnd_ic, parsed.data, sizeof(rnd_ic));

    /*
     * Step 2: answer with our own random values. They are drawn here rather
     * than inside the builder so that the same two can be handed to the
     * verification afterwards.
     */
    emrtd_random_fill(rnd_ifd, sizeof(rnd_ifd));
    emrtd_random_fill(k_ifd, sizeof(k_ifd));
    error = emrtd_bac_build_external_auth(k_enc, k_mac, rnd_ic, rnd_ifd, k_ifd, payload);
    if(error != EmrtdErrorNone) {
        goto cleanup;
    }

    emrtd_apdu_external_authenticate(&apdu, payload, sizeof(payload));
    error = emrtd_apdu_encode(&apdu, command, sizeof(command), &command_len);
    if(error == EmrtdErrorNone) {
        error = emrtd_transceiver_exchange(
            transceiver, command, command_len, response, sizeof(response), &response_len);
    }
    if(error == EmrtdErrorNone) {
        error = emrtd_apdu_decode(response, response_len, &parsed);
    }
    if(error == EmrtdErrorNone && !emrtd_apdu_is_success(&parsed)) {
        /*
         * 6300 is what a chip sends when our cryptogram did not verify under
         * the key it derived from its own MRZ, which is to say that the MRZ
         * the user typed does not belong to this document.
         */
        error = (parsed.sw >> 8) == 0x63 ? EmrtdErrorWrongKey : emrtd_error_from_sw(parsed.sw);
    }
    if(error != EmrtdErrorNone) {
        goto cleanup;
    }

    error = emrtd_bac_process_response(
        k_enc, k_mac, parsed.data, parsed.data_len, rnd_ic, rnd_ifd, k_ifd, out_session);
    if(error == EmrtdErrorNone && out_outcome != NULL) {
        memset(out_outcome, 0, sizeof(*out_outcome));
        snprintf(out_outcome->protocol, sizeof(out_outcome->protocol), "BAC");
        snprintf(out_outcome->summary, sizeof(out_outcome->summary), "BAC 3DES, MRZ key");
    }

cleanup:
    emrtd_secure_wipe(k_enc, sizeof(k_enc));
    emrtd_secure_wipe(k_mac, sizeof(k_mac));
    emrtd_secure_wipe(rnd_ifd, sizeof(rnd_ifd));
    emrtd_secure_wipe(k_ifd, sizeof(k_ifd));
    emrtd_secure_wipe(payload, sizeof(payload));
    if(error != EmrtdErrorNone) {
        emrtd_sm_clear(out_session);
    }
    return error;
}

const EmrtdAccessDriver emrtd_access_driver_bac = {
    .name = "BAC",
    .probe = emrtd_access_bac_probe,
    .authenticate = emrtd_access_bac_authenticate,
    /*
     * BAC runs against the application that is already selected and leaves it
     * selected, so the reader can start reading files straight away.
     */
    .reselect_application = false,
};
