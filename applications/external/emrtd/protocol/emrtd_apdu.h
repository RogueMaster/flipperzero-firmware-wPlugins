/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Command and response APDUs, ISO/IEC 7816-4, and the small set of commands
 * ICAO Doc 9303 builds out of them.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../emrtd_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Le is absent from the command. */
#define EMRTD_LE_NONE (-1)
/** Le = 0 on the wire, meaning "up to 256 bytes". */
#define EMRTD_LE_MAX  256

/** A command APDU before serialisation. */
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    const uint8_t* data;
    size_t data_len;
    int le; /**< EMRTD_LE_NONE, or 1..65536. */
} EmrtdCommandApdu;

/** A parsed response APDU. */
typedef struct {
    const uint8_t* data; /**< Points into the caller's receive buffer. */
    size_t data_len;
    uint16_t sw;
} EmrtdResponseApdu;

/**
 * Serialise a command, choosing short or extended length automatically.
 *
 * @param[out] out      buffer for the encoded APDU
 * @param[in]  out_size its capacity
 * @param[out] out_len  bytes written
 */
EmrtdError emrtd_apdu_encode(
    const EmrtdCommandApdu* command,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);

/** Split a raw response into its data and its status word. */
EmrtdError emrtd_apdu_decode(const uint8_t* raw, size_t raw_len, EmrtdResponseApdu* out);

/** True when the status word is 9000. */
static inline bool emrtd_apdu_is_success(const EmrtdResponseApdu* response) {
    return response->sw == 0x9000;
}

/* --- The commands of ICAO Doc 9303 ------------------------------------- */

/** Application identifier of the eMRTD application (9303-11, 4.2). */
extern const uint8_t EMRTD_AID[7];

/** File identifier of EF.CardAccess, readable without authentication. */
#define EMRTD_FID_CARD_ACCESS 0x011C

void emrtd_apdu_select_application(EmrtdCommandApdu* out, const uint8_t* aid, size_t aid_len);
void emrtd_apdu_select_file(EmrtdCommandApdu* out, uint8_t fid[2]);
void emrtd_apdu_get_challenge(EmrtdCommandApdu* out);
void emrtd_apdu_external_authenticate(EmrtdCommandApdu* out, const uint8_t* data, size_t len);
/**
 * READ BINARY with the offset in P1-P2, ISO/IEC 7816-4 section 7.2.2.
 *
 * Bit 8 of P1 is reserved, so this form addresses at most 32767 bytes into a
 * file. @p offset is masked to those fifteen bits; a caller that may go
 * further asks emrtd_apdu_offset_is_short() first and falls back to
 * emrtd_apdu_read_binary_odd(), which DG2 regularly needs.
 */
void emrtd_apdu_read_binary(EmrtdCommandApdu* out, uint16_t offset, size_t length);
void emrtd_apdu_read_binary_sfi(EmrtdCommandApdu* out, uint8_t sfi, uint8_t offset, size_t length);

/** True when @p offset still fits the fifteen bits P1-P2 leaves for it. */
static inline bool emrtd_apdu_offset_is_short(uint32_t offset) {
    return offset <= 0x7FFF;
}

/** Bytes emrtd_apdu_read_binary_odd() needs for the offset data object. */
#define EMRTD_OFFSET_DO_MAX 5

/**
 * READ BINARY with the odd instruction B1, ISO/IEC 7816-4 section 7.2.3.
 *
 * This is how the rest of a file beyond 32767 bytes is reached: the offset
 * travels in the command data as DO '54' instead of in P1-P2, and the answer
 * comes back wrapped in DO '53', which the caller unwraps with
 * emrtd_tlv_find(response, response_len, 0x53, ...). The wrapper costs a few
 * bytes of the response, so @p length should already allow for it.
 *
 * @param[out] offset_do      scratch the data object is built in; the command
 *                            only points at it, so it has to stay alive until
 *                            the command has been encoded
 * @param[in]  offset_do_size its capacity, at least EMRTD_OFFSET_DO_MAX
 */
EmrtdError emrtd_apdu_read_binary_odd(
    EmrtdCommandApdu* out,
    uint32_t offset,
    size_t length,
    uint8_t* offset_do,
    size_t offset_do_size);

void emrtd_apdu_mse_set_at(EmrtdCommandApdu* out, const uint8_t* data, size_t len);
void emrtd_apdu_general_authenticate(
    EmrtdCommandApdu* out,
    const uint8_t* data,
    size_t len,
    bool chaining);

#ifdef __cplusplus
}
#endif
