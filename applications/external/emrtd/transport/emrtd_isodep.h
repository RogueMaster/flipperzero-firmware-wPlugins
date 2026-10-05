/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * ISO-DEP, the block transmission protocol of ISO/IEC 14443-4, implemented
 * here rather than taken from the firmware.
 *
 * The Flipper has an ISO 14443-4A poller of its own and this reader used it
 * at first. It cannot be used for a travel document, for one reason that is
 * not a matter of taste:
 *
 *     uint32_t iso14443_4a_get_fwt_fc_max(const Iso14443_4aData* data) {
 *         uint32_t fwt_fc_max = ISO14443_4A_FDT_DEFAULT_FC;
 *         ...
 *         fwt_fc_max = 4096UL << fwi;
 *
 * with ISO14443_4A_FDT_DEFAULT_FC defined as ISO14443_3A_FDT_POLL_FC, which
 * is 1620 carrier cycles - 120 microseconds. That is the ISO 14443-3 poll
 * frame delay time, not a frame waiting time, and it is what every I-block
 * gets whenever the card's ATS carries no TB1. No smartcard answers anything
 * in 120 microseconds. Its RATS window is fixed at 40000 cycles as well,
 * which is below the 65536 that ISO/IEC 14443-4 allows a card for the answer
 * to RATS. Neither value is reachable from an application, and a chip that
 * loses either race is reported as a card that has been moved away.
 *
 * The Python reference this reader was ported from hit exactly this and
 * solved it the same way: it drives bare ISO 14443-3A frames and asks for a
 * frame waiting time of four million cycles, about 295 milliseconds, because
 * "a passport with FWI=9 needs roughly 150 ms to answer". This file is that
 * decision brought into the application.
 *
 * What it owns, therefore, is everything above the bare frame: RATS and the
 * ATS, the block number and when it toggles, chaining in both directions,
 * the waiting time extension a chip asks for while it does elliptic curve
 * arithmetic, recovery of an answer that was lost on the way, and the
 * deselect at the end. It is handed one function that puts a frame on the
 * wire and brings the answer back, which is what makes the whole layer
 * testable on a host against a simulated chip.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../emrtd_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Timings are in carrier cycles, the unit the radio takes. One cycle of the
 * 13.56 MHz carrier is about 73.7 nanoseconds.
 */

/**
 * How long the card has to answer RATS.
 *
 * ISO/IEC 14443-4 gives it 65536 cycles, 4.8 milliseconds. This is fifteen
 * times that, because the only cost of waiting longer is a slower failure on
 * a card that is not there, and the cost of waiting too little is a document
 * that cannot be read at all.
 */
#define EMRTD_ISODEP_RATS_FWT_FC 1000000u

/**
 * Floor under the frame waiting time, whatever the ATS asked for.
 *
 * FWI 9, which is what a passport typically announces, works out at 2097152
 * cycles - 155 milliseconds - and the reference implementation found that to
 * be marginal in practice on this hardware. This is the value it settled on.
 */
#define EMRTD_ISODEP_FWT_MIN_FC 4000000u

/** Ceiling, from FWI 14: the largest waiting time ISO/IEC 14443-4 defines. */
#define EMRTD_ISODEP_FWT_MAX_FC (4096u << 14)

/** FSDI 8, the frame size this reader announces in RATS. */
#define EMRTD_ISODEP_FSD 256

/** One frame, protocol control byte and information field, without the CRC. */
#define EMRTD_ISODEP_FRAME_MAX 256

/** An ATS is at most 20 bytes plus its length byte; this rounds it up. */
#define EMRTD_ISODEP_ATS_MAX 24

/**
 * Margin added to the start-up guard time the card asks for.
 *
 * The standard asks for a minimum and says nothing about what happens to a
 * reader that is a few microseconds under it. Two milliseconds costs nothing
 * once per read and puts the question beyond doubt.
 */
#define EMRTD_ISODEP_SFGT_MARGIN_MS 2

/**
 * How many times a chip may ask for more time before the reader gives up.
 *
 * Each round is worth up to the waiting time again, so this is a guard
 * against a chip that never finishes rather than a tight budget.
 */
#define EMRTD_ISODEP_WTX_ROUNDS_MAX 60

/**
 * Rounds of recovery for a block whose answer did not arrive intact.
 *
 * ISO/IEC 14443-4 section 7.5.5 puts recovery on the reader, and the block
 * that does it is never the lost block itself. Rule D has the card toggle its
 * block number on every I-block it receives, "independent of its block
 * number", and execute it: an I-block sent twice is two commands. Under Secure
 * Messaging the second one carries a send sequence counter the chip has
 * already moved past, so it fails its checksum and the session ends, and a
 * PACE step run twice leaves the protocol in a state the chip refuses to
 * continue from.
 *
 * So a lost answer is asked for with R(NAK), rule 4, or with the same R(ACK)
 * while the card is chaining its answer, rule 5. A card that had the block
 * sends its last answer again, rule 11, without executing anything; one that
 * never had it says so with an R(ACK) carrying the other block number, rule
 * 12, and only then is the block itself sent again, rule 6.
 */
#define EMRTD_ISODEP_RETRIES 2

/**
 * Put one bare frame on the wire and bring the answer back.
 *
 * The frame is the protocol control byte and what follows it. The cyclic
 * redundancy check belongs to the layer below and is neither written nor
 * expected here.
 *
 * @param[in]  context  implementation context
 * @param[in]  tx       frame to send
 * @param[in]  tx_len   its length
 * @param[out] rx       buffer for the answer
 * @param[in]  rx_cap   capacity of @p rx
 * @param[out] rx_len   bytes written
 * @param[in]  fwt_fc   how long to wait for the answer, in carrier cycles
 */
typedef EmrtdError (*EmrtdIsoDepFrameFn)(
    void* context,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t fwt_fc);

/**
 * Wait, because the card asked to be left alone for a moment.
 *
 * Only the start-up guard time needs this, and only once per activation. It is
 * a hook rather than a call to the firmware so that this file stays free of
 * furi.h and the guard time can be checked on a host without anybody sleeping.
 */
typedef void (*EmrtdIsoDepDelayFn)(void* context, uint32_t ms);

typedef struct {
    EmrtdIsoDepFrameFn send;
    EmrtdIsoDepDelayFn delay;
    void* context;

    bool activated;
    uint8_t block_number; /**< Toggles after every accepted exchange. */
    uint16_t fsc; /**< Card frame size, from the ATS. */
    uint8_t fwi; /**< Frame waiting time index, from TB1. */
    uint32_t fwt_fc; /**< What the reader actually waits, after the floor. */
    bool fwi_announced; /**< False when the ATS carried no TB1. */

    uint8_t sfgi; /**< Start-up frame guard time index, from TB1. */
    uint32_t sfgt_ms; /**< What that works out at, margin included. */

    uint8_t ats[EMRTD_ISODEP_ATS_MAX];
    size_t ats_len;

    /* One frame each way. Held here so that no path needs the stack for it. */
    uint8_t tx_frame[EMRTD_ISODEP_FRAME_MAX];
    uint8_t rx_frame[EMRTD_ISODEP_FRAME_MAX];
} EmrtdIsoDep;

/**
 * Bind the layer to a frame transport. Does not touch the card.
 *
 * @param[in] delay may be NULL, in which case the start-up guard time a card
 *                  asks for is computed and reported but not waited out
 */
void emrtd_isodep_init(
    EmrtdIsoDep* instance,
    EmrtdIsoDepFrameFn send,
    EmrtdIsoDepDelayFn delay,
    void* context);

/** Forget the session. The frame transport is left bound. */
void emrtd_isodep_reset(EmrtdIsoDep* instance);

/**
 * Send RATS, parse the ATS, and wait out the card's start-up guard time.
 *
 * On success the card is in the ISO 14443-4 protocol state and
 * emrtd_isodep_transceive() may be called.
 *
 * The wait is not optional politeness. ISO/IEC 14443-4 section 5.2.5 makes
 * SFGT the minimum time between the end of the ATS and the reader's first
 * frame, and a travel document asks for a long one because it is starting an
 * operating system: SFGI 6, which is 19 milliseconds, has been seen in the
 * field. Nothing in the Flipper firmware honours it - there is not one
 * mention of SFG in its ISO 14443-4A implementation - so a reader that talks
 * straight after the ATS is talking to a chip that is not listening yet.
 */
EmrtdError emrtd_isodep_activate(EmrtdIsoDep* instance);

/**
 * Exchange one APDU, chaining it in either direction as the frame size needs.
 *
 * @param[out] rx_len bytes of response, status word included
 */
EmrtdError emrtd_isodep_transceive(
    EmrtdIsoDep* instance,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len);

/**
 * Release the card with S(DESELECT).
 *
 * Failure is ignored: the field is about to go down, which releases it
 * anyway, and there is nothing useful to report to a caller that is already
 * finishing.
 */
void emrtd_isodep_deselect(EmrtdIsoDep* instance);

/** The card's frame size, capped at what this reader can send. */
uint16_t emrtd_isodep_fsc(const EmrtdIsoDep* instance);

/** The start-up guard time this card asked for, margin included, in ms. */
uint32_t emrtd_isodep_sfgt_ms(const EmrtdIsoDep* instance);

/** The raw ATS, for the log and the trace. NULL until activation succeeds. */
const uint8_t* emrtd_isodep_ats(const EmrtdIsoDep* instance, size_t* len);

/** Parse an ATS into the fields this layer needs. Exposed for the tests. */
void emrtd_isodep_parse_ats(EmrtdIsoDep* instance, const uint8_t* ats, size_t len);

#ifdef __cplusplus
}
#endif
