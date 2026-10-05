/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Basic Access Control, ICAO Doc 9303 part 11 section 4.3 and appendix D.
 *
 * The chip is asked for a challenge, the reader answers with an encrypted and
 * authenticated blob built from that challenge and its own random values, and
 * both sides derive the same session keys from the two halves.
 *
 * BAC is the older of the two access protocols and is being withdrawn: a
 * document issued after 2017 may implement PACE only, which is exactly why
 * the PACE driver is tried first.
 */
#pragma once

#include "../access/emrtd_access.h"
#include "emrtd_sm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Derive Kseed, K_Enc and K_MAC from the MRZ input. */
EmrtdError emrtd_bac_derive_keys(
    const EmrtdCredentials* credentials,
    uint8_t out_k_enc[16],
    uint8_t out_k_mac[16]);

/**
 * Build the EXTERNAL AUTHENTICATE payload.
 *
 * @p rnd_ifd and @p k_ifd are inputs so that the vector tests can pin them;
 * pass NULL for either to have it drawn from the hardware generator.
 *
 * @param[out] out  40 bytes: E.IFD || M.IFD
 */
EmrtdError emrtd_bac_build_external_auth(
    const uint8_t k_enc[16],
    const uint8_t k_mac[16],
    const uint8_t rnd_ic[8],
    uint8_t rnd_ifd[8],
    uint8_t k_ifd[16],
    uint8_t out[40]);

/**
 * Verify the chip's answer and open the session.
 *
 * Checks M.IC before decrypting anything, then that both random values came
 * back unchanged - a mismatch there means the MRZ input was wrong.
 */
EmrtdError emrtd_bac_process_response(
    const uint8_t k_enc[16],
    const uint8_t k_mac[16],
    const uint8_t* response,
    size_t response_len,
    const uint8_t rnd_ic[8],
    const uint8_t rnd_ifd[8],
    const uint8_t k_ifd[16],
    EmrtdSm* out_session);

#ifdef __cplusplus
}
#endif
