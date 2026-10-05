/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include <stdio.h>
#include <string.h>

#include "../crypto/emrtd_pace.h"
#include "../protocol/emrtd_security_info.h"
#include "emrtd_access.h"

/**
 * Find the PACEInfo this chip wants used, and say why not when there is none.
 *
 * EF.CardAccess is the only place a passport says which PACE protocol it
 * implements, so without it there is nothing to run - the object identifier
 * and the domain parameters are not guessable.
 */
static bool emrtd_access_pace_info(
    const uint8_t* card_access,
    size_t card_access_len,
    EmrtdPaceInfo* out,
    EmrtdError* reason) {
    if(card_access == NULL || card_access_len == 0) {
        if(reason != NULL) {
            *reason = EmrtdErrorNoAccessMethod;
        }
        return false;
    }
    if(!emrtd_security_infos_best_pace(card_access, card_access_len, out)) {
        if(reason != NULL) {
            *reason = EmrtdErrorNoAccessMethod;
        }
        return false;
    }
    if(!out->usable && reason != NULL) {
        /* Names the curve or the mapping the document asked for. */
        *reason = out->reason != EmrtdErrorNone ? out->reason : EmrtdErrorUnsupported;
    }
    return true;
}

/** PACE opens on the CAN when one was entered, otherwise on the MRZ input. */
static bool emrtd_access_pace_has_password(const EmrtdCredentials* credentials) {
    if(credentials == NULL) {
        return false;
    }
    if(credentials->has_can && credentials->can[0] != '\0') {
        return true;
    }
    return emrtd_credentials_valid_mrz(credentials);
}

static EmrtdAccessScore emrtd_access_pace_probe(
    const uint8_t* card_access,
    size_t card_access_len,
    const EmrtdCredentials* credentials,
    EmrtdError* reason) {
    EmrtdPaceInfo info;
    if(!emrtd_access_pace_info(card_access, card_access_len, &info, reason)) {
        return EmrtdAccessScoreUnsupported;
    }
    if(!info.usable) {
        return EmrtdAccessScoreUnsupported;
    }
    if(!emrtd_access_pace_has_password(credentials)) {
        if(reason != NULL) {
            *reason = EmrtdErrorInvalidInput;
        }
        return EmrtdAccessScoreUnsupported;
    }

    /* The chip advertised this protocol itself, so it outranks a guess. */
    return EmrtdAccessScoreAnnounced;
}

static EmrtdError emrtd_access_pace_authenticate(
    EmrtdTransceiver* transceiver,
    const uint8_t* card_access,
    size_t card_access_len,
    const EmrtdCredentials* credentials,
    EmrtdSm* out_session,
    EmrtdAccessOutcome* out_outcome) {
    if(transceiver == NULL || credentials == NULL || out_session == NULL) {
        return EmrtdErrorInternal;
    }

    EmrtdPaceInfo info;
    EmrtdError reason = EmrtdErrorNoAccessMethod;
    if(!emrtd_access_pace_info(card_access, card_access_len, &info, &reason)) {
        return reason;
    }
    if(!info.usable) {
        return reason;
    }

    const EmrtdError error = emrtd_pace_run(transceiver, &info, credentials, out_session);
    if(error != EmrtdErrorNone) {
        return error;
    }

    if(out_outcome != NULL) {
        memset(out_outcome, 0, sizeof(*out_outcome));
        snprintf(out_outcome->protocol, sizeof(out_outcome->protocol), "PACE");

        char description[40];
        emrtd_pace_info_describe(&info, description, sizeof(description));
        snprintf(out_outcome->summary, sizeof(out_outcome->summary), "PACE %s", description);
    }
    return EmrtdErrorNone;
}

const EmrtdAccessDriver emrtd_access_driver_pace = {
    .name = "PACE",
    .probe = emrtd_access_pace_probe,
    .authenticate = emrtd_access_pace_authenticate,
    /*
     * PACE is run against the master file, because EF.CardAccess lives there
     * and the protocol is not part of the eMRTD application. The application
     * therefore has to be selected again once the session exists, this time
     * inside it.
     */
    .reselect_application = true,
};
