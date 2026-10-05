/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * SecurityInfos, ICAO Doc 9303 part 11 section 9.2 and appendix A.
 *
 * EF.CardAccess is readable before any authentication and is the chip telling
 * the reader how it wants to be opened. DG14 carries the same structure once
 * the session exists. Both are a SET OF SecurityInfo, each a SEQUENCE that
 * starts with a protocol object identifier.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../crypto/emrtd_crypto.h"
#include "../crypto/emrtd_ec.h"
#include "../emrtd_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest object identifier body this reader stores. */
#define EMRTD_OID_MAX 16

/** How the PACE nonce is mapped onto the curve. */
typedef enum {
    EmrtdPaceMappingUnknown,
    EmrtdPaceMappingGeneric, /**< GM - the only one implemented. */
    EmrtdPaceMappingIntegrated, /**< IM. */
    EmrtdPaceMappingChipAuth, /**< CAM. */
} EmrtdPaceMapping;

/** The key agreement family. */
typedef enum {
    EmrtdPaceAgreementUnknown,
    EmrtdPaceAgreementEcdh,
    EmrtdPaceAgreementDh, /**< MODP groups; impossible here, see docs/platform.md. */
} EmrtdPaceAgreement;

/** One PACEInfo entry. */
typedef struct {
    uint8_t oid[EMRTD_OID_MAX];
    size_t oid_len;
    const char* oid_name; /**< e.g. "id-PACE-ECDH-GM-AES-CBC-CMAC-128". */
    uint32_t version;
    uint8_t parameter_id; /**< TR-03110-3 table 4; 0xFF when absent. */
    EmrtdPaceMapping mapping;
    EmrtdPaceAgreement agreement;
    EmrtdCipher cipher;
    const EmrtdCurve* curve; /**< NULL when the parameter id is unknown. */
    bool usable; /**< True when this build can actually run it. */
    EmrtdError reason; /**< Why not, when @c usable is false. */
} EmrtdPaceInfo;

/** A summary of everything EF.CardAccess or DG14 announced. */
typedef struct {
    EmrtdPaceInfo pace; /**< The best PACEInfo found. */
    bool has_pace;
    bool has_chip_auth;
    bool has_terminal_auth;
    bool has_active_auth;
    uint8_t entry_count;
} EmrtdSecurityInfos;

/** Walk a SecurityInfos structure. Tolerates trailing rubbish. */
EmrtdError emrtd_security_infos_parse(const uint8_t* data, size_t len, EmrtdSecurityInfos* out);

/**
 * Pick the PACEInfo this reader should act on.
 *
 * Where a chip announces several, the usable ones win over the unusable, and
 * a stronger cipher wins over a weaker one. When none is usable the best
 * unusable entry is still returned so that the error can name it.
 */
bool emrtd_security_infos_best_pace(const uint8_t* data, size_t len, EmrtdPaceInfo* out);

/** Describe a PACEInfo in one line, e.g. "ECDH-GM/AES-128, brainpoolP256r1". */
void emrtd_pace_info_describe(const EmrtdPaceInfo* info, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif
