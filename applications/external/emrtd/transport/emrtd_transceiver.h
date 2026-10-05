/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The one thing every layer above needs from the hardware: move an APDU and
 * bring back the answer.
 *
 * An eMRTD may sit on a type A or a type B chip, and the test suite drives the
 * whole stack against a simulated one, so the reader never talks to a poller
 * directly. It calls through this port.
 *
 * Frame sizes are not a detail that can be guessed. The Flipper announces
 * FSD = 256 bytes in RATS, and on type B the firmware's ISO 14443-4 layer does
 * not reassemble a chained response, so a command whose answer would exceed
 * one frame has to be split by us. The type A path does reassemble one, since
 * it owns the protocol (see emrtd_isodep.h), but the limit is kept for both so
 * that there is one rule rather than two. @c fsc is what the card announced it
 * can receive, @c fsd what the reader can, and emrtd_transceiver_max_le()
 * turns the pair into the largest Le that is safe to ask for.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../emrtd_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest APDU either direction, which is what the frame size allows. */
#define EMRTD_APDU_MAX_SIZE 256

typedef struct EmrtdTransceiver EmrtdTransceiver;

/** Implementation hooks. */
typedef struct {
    /** Human readable name of the backing protocol, e.g. "ISO 14443-4A". */
    const char* name;

    /**
     * Exchange one APDU.
     *
     * @param[in]  ctx      implementation context
     * @param[in]  tx       command APDU
     * @param[in]  tx_len   its length
     * @param[out] rx       buffer for the response, status word included
     * @param[in]  rx_cap   capacity of @p rx
     * @param[out] rx_len   bytes written
     */
    EmrtdError (*transceive)(
        void* ctx,
        const uint8_t* tx,
        size_t tx_len,
        uint8_t* rx,
        size_t rx_cap,
        size_t* rx_len);
} EmrtdTransceiverApi;

struct EmrtdTransceiver {
    const EmrtdTransceiverApi* api;
    void* ctx;
    uint16_t fsc; /**< Card frame size, from the ATS. Defaults to 32 per ISO 14443-4. */
    uint16_t fsd; /**< Reader frame size, 256 on the Flipper. */
};

/** Exchange one APDU. Thin wrapper over the vtable, with the argument checks. */
EmrtdError emrtd_transceiver_exchange(
    EmrtdTransceiver* transceiver,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len);

/**
 * Largest Le that will come back in a single frame.
 *
 * Leaves room for the Secure Messaging envelope - DO'87' with its padding
 * indicator and length, DO'99' and DO'8E' - and rounds down to the cipher
 * block so that the padded plaintext fits exactly.
 *
 * @param[in] block_size cipher block size, or 0 before a session exists
 */
size_t emrtd_transceiver_max_le(const EmrtdTransceiver* transceiver, size_t block_size);

/** Largest command data field the card will accept in a single frame. */
size_t emrtd_transceiver_max_lc(const EmrtdTransceiver* transceiver);

/** Decode the FSC announced by an ATS format byte T0 (ISO 14443-4 table 3). */
uint16_t emrtd_transceiver_fsc_from_fsci(uint8_t fsci);

#ifdef __cplusplus
}
#endif
