/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Access control drivers.
 *
 * A passport opens with one of two protocols, and which one it wants is not
 * something the person holding it knows. Rather than ask, the reader keeps a
 * registry of drivers, asks each what it makes of the chip's EF.CardAccess,
 * and runs the most promising one; if that fails it falls back to the next.
 * Adding a further protocol - PACE with the integrated mapping, say - means
 * adding a driver, not editing the reader.
 */
#pragma once

#include "../crypto/emrtd_sm.h"
#include "../emrtd_error.h"
#include "../transport/emrtd_transceiver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMRTD_DOC_NUMBER_MAX 20
#define EMRTD_DATE_LEN       6 /**< YYMMDD. */
#define EMRTD_CAN_MAX        6

/** What the person read off the data page. */
typedef struct {
    char document_number[EMRTD_DOC_NUMBER_MAX + 1];
    char date_of_birth[EMRTD_DATE_LEN + 1];
    char date_of_expiry[EMRTD_DATE_LEN + 1];
    char can[EMRTD_CAN_MAX + 1]; /**< Card Access Number, PACE only. */
    bool has_can;
} EmrtdCredentials;

/** True when the three MRZ fields are present and well formed. */
bool emrtd_credentials_valid_mrz(const EmrtdCredentials* credentials);

/** What a driver worked out about the chip, for the UI and the report. */
typedef struct {
    char summary[48]; /**< e.g. "PACE ECDH-GM/AES-128, brainpoolP256r1". */
    char protocol[16]; /**< "PACE" or "BAC". */
} EmrtdAccessOutcome;

/** How well a driver thinks it fits this chip. */
typedef enum {
    EmrtdAccessScoreUnsupported = 0, /**< Do not attempt. */
    EmrtdAccessScorePossible = 1, /**< Might work; no evidence either way. */
    EmrtdAccessScoreAnnounced = 2, /**< The chip advertises this protocol. */
} EmrtdAccessScore;

typedef struct EmrtdAccessDriver EmrtdAccessDriver;

struct EmrtdAccessDriver {
    /** Short name for the menu, e.g. "PACE". */
    const char* name;

    /**
     * Judge the chip from its EF.CardAccess.
     *
     * @param[in]  card_access     the file, or NULL when it could not be read
     * @param[in]  card_access_len its length
     * @param[in]  credentials     what the user supplied
     * @param[out] reason          why an unsupported verdict was reached, may be NULL
     */
    EmrtdAccessScore (*probe)(
        const uint8_t* card_access,
        size_t card_access_len,
        const EmrtdCredentials* credentials,
        EmrtdError* reason);

    /**
     * Run the protocol and open a Secure Messaging session.
     *
     * Called with the eMRTD application already selected.
     */
    EmrtdError (*authenticate)(
        EmrtdTransceiver* transceiver,
        const uint8_t* card_access,
        size_t card_access_len,
        const EmrtdCredentials* credentials,
        EmrtdSm* out_session,
        EmrtdAccessOutcome* out_outcome);

    /** True when the application has to be selected again afterwards. */
    bool reselect_application;
};

/** Which driver the user asked for. */
typedef enum {
    EmrtdAccessMethodAuto, /**< Highest scoring driver first, then the rest. */
    EmrtdAccessMethodPace,
    EmrtdAccessMethodBac,
} EmrtdAccessMethod;

const char* emrtd_access_method_name(EmrtdAccessMethod method);

/** The registry, in preference order. */
const EmrtdAccessDriver* emrtd_access_driver_at(size_t index);
size_t emrtd_access_driver_count(void);

/** The driver a fixed choice maps to, or NULL for EmrtdAccessMethodAuto. */
const EmrtdAccessDriver* emrtd_access_driver_for_method(EmrtdAccessMethod method);

extern const EmrtdAccessDriver emrtd_access_driver_pace;
extern const EmrtdAccessDriver emrtd_access_driver_bac;

#ifdef __cplusplus
}
#endif
