/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Secure Messaging, ICAO Doc 9303 part 11 section 9.8.
 *
 * Once BAC or PACE has succeeded every APDU travels inside a protected
 * envelope: the command data encrypted in DO'87', the expected length in
 * DO'97', a checksum over the lot in DO'8E'. The response carries DO'87',
 * the real status word in DO'99' and its own DO'8E'.
 *
 * The Send Sequence Counter is incremented before the command and again
 * before the response, so the two sides stay in step; losing count is
 * unrecoverable and the session has to be torn down.
 *
 * One detail is worth stating because it is easy to get wrong and expensive to
 * debug against a real chip: the MAC input SSC || M is padded with ISO 9797-1
 * method 2 for *both* cipher families. That is what the appendix D vectors
 * show for 3DES and what a real passport does for AES.
 */
#pragma once

#include "../protocol/emrtd_apdu.h"
#include "emrtd_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The state of a protected session. */
typedef struct {
    EmrtdCipher cipher;
    uint8_t ks_enc[EMRTD_KEY_MAX_SIZE];
    uint8_t ks_mac[EMRTD_KEY_MAX_SIZE];
    uint8_t ssc[EMRTD_BLOCK_MAX_SIZE]; /**< Big endian, block sized. */
    bool established;
} EmrtdSm;

/**
 * Open a session.
 *
 * @param[in] ssc  initial counter, @p cipher's block size in bytes, big
 *                 endian; PACE starts at zero, BAC at RND.IC[4..8] || RND.IFD[4..8]
 */
void emrtd_sm_init(
    EmrtdSm* sm,
    EmrtdCipher cipher,
    const uint8_t* ks_enc,
    const uint8_t* ks_mac,
    const uint8_t* ssc);

/** Wipe the keys and the counter. */
void emrtd_sm_clear(EmrtdSm* sm);

/**
 * Wrap a command APDU.
 *
 * Increments the SSC. The result is written to @p out as a complete,
 * serialised APDU.
 */
EmrtdError emrtd_sm_protect(
    EmrtdSm* sm,
    const EmrtdCommandApdu* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);

/**
 * Verify and unwrap a response.
 *
 * Increments the SSC, checks DO'8E' before anything is decrypted, and writes
 * the plaintext to @p out. @p response may alias @p out.
 */
EmrtdError emrtd_sm_unprotect(
    EmrtdSm* sm,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_size,
    EmrtdResponseApdu* out_apdu);

/** Overhead emrtd_sm_protect() adds to a command carrying @p data_len bytes. */
size_t emrtd_sm_command_overhead(const EmrtdSm* sm, size_t data_len);

#ifdef __cplusplus
}
#endif
