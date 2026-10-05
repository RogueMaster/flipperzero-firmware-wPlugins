/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * PACE with the generic mapping over ECDH, ICAO Doc 9303 part 11 section 4.4
 * and appendix G.
 *
 * The chip sends a nonce encrypted under a key derived from the password; an
 * ephemeral key exchange maps that nonce onto a fresh generator; a second
 * exchange over the mapped generator produces the session keys; and both sides
 * prove they got the same answer by exchanging authentication tokens.
 *
 * What this reader can and cannot do is a property of the platform, not of the
 * protocol, and is spelled out in docs/platform.md: the generic mapping over
 * ECDH on curves up to 256 bits. The integrated mapping and PACE over MODP
 * groups are detected and refused by name rather than failing obscurely.
 */
#pragma once

#include "../access/emrtd_access.h"
#include "../protocol/emrtd_security_info.h"
#include "emrtd_sm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Derive Kpi, the key the encrypted nonce is protected with.
 *
 * From the CAN when one was supplied, otherwise from the MRZ information.
 * ICAO 9303-11 section 9.7.3, table 11.
 */
EmrtdError emrtd_pace_password_key(const EmrtdCredentials* credentials, uint8_t out[16]);

/**
 * Build the input to an authentication token.
 *
 *     7F49 { 06 <oid> 86 <peer public point> }
 *
 * ICAO 9303-11 section 4.4.3.4.
 */
EmrtdError emrtd_pace_token_input(
    const uint8_t* oid,
    size_t oid_len,
    const uint8_t* point,
    size_t point_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);

/** Run the whole exchange. This is what the PACE access driver calls. */
EmrtdError emrtd_pace_run(
    EmrtdTransceiver* transceiver,
    const EmrtdPaceInfo* info,
    const EmrtdCredentials* credentials,
    EmrtdSm* out_session);

#ifdef __cplusplus
}
#endif
