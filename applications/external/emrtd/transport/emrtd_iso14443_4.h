/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * An EmrtdTransceiver backed by the Flipper's NFC pollers.
 *
 * A travel document chip answers on either of the two ISO 14443 flavours and
 * the holder has no way of knowing which; European identity cards in
 * particular are frequently type B while passports are usually type A. The
 * difference is confined to this file and everything above it sees one port.
 *
 * The two flavours are not reached the same way, and the reason is measured
 * rather than stylistic. On type A this reader drives the ISO 14443-3A poller
 * and runs the block transmission protocol itself, in emrtd_isodep.c, because
 * the firmware's own ISO 14443-4A poller gives every block a frame waiting
 * time of 1620 carrier cycles - 120 microseconds - whenever the card's ATS
 * carries no TB1, and allows only 40000 cycles for the answer to RATS. A
 * passport loses both races while lying perfectly still. Type B keeps the
 * firmware poller, whose waiting time comes from the ATQB and is correct.
 *
 * One consequence is worth stating: because the type A path owns the protocol,
 * it reassembles a chained response. The type B path still cannot, so
 * emrtd_transceiver_max_le() continues to size every question so that the
 * answer fits one frame.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#include "emrtd_transceiver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Largest INF field the type B path will put on, or expect off, the wire.
 *
 * The type B poller assembles PCB || INF into a 256 byte BitBuffer and then
 * appends the two CRC bytes inside another one, so 253 is the largest INF
 * that cannot overflow either. BitBuffer overflow is a furi_check(), which
 * aborts the application rather than returning an error, so this bound is
 * enforced before every append instead of being assumed.
 */
#define EMRTD_ISO14443_4_MAX_INF 253

/** Frame size the Flipper announces in RATS and in ATTRIB. */
#define EMRTD_ISO14443_4_FSD 256

/** Capacity of the working BitBuffers; comfortably above one frame. */
#define EMRTD_ISO14443_4_BUFFER_SIZE 512

/** Which poller is behind the port. */
typedef enum {
    EmrtdIso14443_4VariantNone,
    EmrtdIso14443_4VariantA,
    EmrtdIso14443_4VariantB,
} EmrtdIso14443_4Variant;

/**
 * Called with every block of bytes that crosses the interface.
 *
 * Runs on the NFC thread, inside the poller callback, so an implementation
 * has to be quick and must not touch the user interface.
 *
 * @param[in] outgoing true for a command, false for the response to it
 */
typedef void (
    *EmrtdIso14443_4TraceCallback)(void* context, bool outgoing, const uint8_t* data, size_t len);

typedef struct EmrtdIso14443_4 EmrtdIso14443_4;

EmrtdIso14443_4* emrtd_iso14443_4_alloc(void);
void emrtd_iso14443_4_free(EmrtdIso14443_4* instance);

/**
 * Point the port at an activated type A poller and open an ISO-DEP session.
 *
 * The poller has completed anticollision and selection; RATS is sent from
 * here, with a waiting time this reader chooses. A failure means the chip
 * would not enter the protocol state, and the field has to be dropped before
 * it is worth trying again - a card that did answer RATS will ignore a second
 * one.
 */
EmrtdError emrtd_iso14443_4_bind_3a(EmrtdIso14443_4* instance, Iso14443_3aPoller* poller);

/** Point the port at an activated type B poller. */
void emrtd_iso14443_4_bind_4b(
    EmrtdIso14443_4* instance,
    Iso14443_4bPoller* poller,
    const Iso14443_4bData* data);

/**
 * Forget the poller.
 *
 * The pollers belong to the NFC stack and are destroyed when the session ends,
 * so the port has to be told to stop holding on to them. A type A session is
 * released with S(DESELECT) on the way out, which lets the chip go back to
 * idle instead of waiting out the field.
 */
void emrtd_iso14443_4_unbind(EmrtdIso14443_4* instance);

/** The port itself, ready to be handed to the access drivers. */
EmrtdTransceiver* emrtd_iso14443_4_transceiver(EmrtdIso14443_4* instance);

/** Which poller is bound, for the report and for the error messages. */
EmrtdIso14443_4Variant emrtd_iso14443_4_variant(const EmrtdIso14443_4* instance);

/**
 * Describe the activated card in one line, for the log and the trace.
 *
 * Names the flavour, the frame size, and on type A the raw ATS and the frame
 * waiting time in milliseconds - which is the first thing worth knowing when
 * a read fails on a document that has not moved.
 */
void emrtd_iso14443_4_describe(const EmrtdIso14443_4* instance, char* out, size_t out_size);

/**
 * Say what went wrong, with the radio's own code, for the trace.
 *
 * One EmrtdError covers several very different radio failures, and which one
 * it was is the first question worth asking about a read that stopped.
 */
void emrtd_iso14443_4_failure_detail(
    const EmrtdIso14443_4* instance,
    EmrtdError error,
    char* out,
    size_t out_size);

/**
 * Say what the radio lost during an exchange that nevertheless succeeded.
 *
 * An answer that went missing and was asked for again leaves no mark on the
 * APDU, so without this a document that reads only just - because it sits at
 * the edge of the field, or draws more than the field gives while it computes
 * - looks exactly like one that reads comfortably.
 *
 * @return false when every frame of the last exchange arrived first time, in
 *         which case @p out is left empty
 */
bool emrtd_iso14443_4_recovery_detail(const EmrtdIso14443_4* instance, char* out, size_t out_size);

/** Install the diagnostic hook. Pass NULL to remove it. */
void emrtd_iso14443_4_set_trace(
    EmrtdIso14443_4* instance,
    EmrtdIso14443_4TraceCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
