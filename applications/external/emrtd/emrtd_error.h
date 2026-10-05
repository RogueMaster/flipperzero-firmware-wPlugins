/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * One error code for every layer of the application.
 *
 * The reader runs unattended on a device with a four line screen, so an error
 * has to say what went wrong in words the person holding the document can act
 * on. emrtd_error_text() is the heading that says what happened, and
 * emrtd_error_hint() the three lines under it that say what to try.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EmrtdErrorNone = 0,

    /* Transport and card */
    EmrtdErrorNoCard, /**< Nothing in the field. */
    EmrtdErrorCardLost, /**< The document moved away mid read. */
    EmrtdErrorActivation, /**< The chip answered, but would not open a session. */
    EmrtdErrorTransport, /**< The RF exchange failed. */
    EmrtdErrorProtocol, /**< The card broke ISO 14443-4. */
    EmrtdErrorNotEmrtd, /**< The chip has no eMRTD application. */

    /* APDU level */
    EmrtdErrorApdu, /**< The card answered with a status word other than 9000. */
    EmrtdErrorFileNotFound, /**< 6A82 - the file is not on this chip. */
    EmrtdErrorAccessDenied, /**< 6982/6983 - not allowed, or the chip is blocked. */

    /* Access control */
    EmrtdErrorWrongKey, /**< The MRZ input or the CAN does not open the chip. */
    EmrtdErrorNoAccessMethod, /**< Neither PACE nor BAC could be established. */
    EmrtdErrorPaceUnsupportedCurve, /**< The chip asks for a curve this build cannot compute. */
    EmrtdErrorPaceUnsupportedMapping, /**< Integrated or chip authentication mapping. */
    EmrtdErrorPaceUnsupportedDh, /**< PACE over MODP groups; see docs/platform.md. */
    EmrtdErrorPaceFailed, /**< The chip's authentication token did not match. */

    /* Secure messaging */
    EmrtdErrorSecureMessaging, /**< The response MAC or padding did not verify. */

    /* Parsing */
    EmrtdErrorParse, /**< The file is not the structure the standard describes. */
    EmrtdErrorUnsupported, /**< Understood, but out of this reader's scope. */

    /* Local */
    EmrtdErrorOutOfMemory, /**< The heap cannot hold a read; see docs/platform.md. */
    EmrtdErrorStorage, /**< The SD card refused the export. */
    EmrtdErrorBufferTooSmall, /**< A caller supplied buffer could not hold the result. */
    EmrtdErrorInvalidInput, /**< The credentials are not well formed. */
    EmrtdErrorCancelled, /**< The user left the read scene. */
    EmrtdErrorInternal, /**< A library call failed in a way that should not happen. */

    EmrtdErrorCount,
} EmrtdError;

/**
 * The heading: one line that fits the screen in FontPrimary, at most 124 px of
 * glyph advances, which is about twenty characters.
 */
const char* emrtd_error_text(EmrtdError error);

/**
 * What to try next, already broken into at most three lines with '\n'.
 *
 * Each line stays within 120 px of FontSecondary, so neither the error scene
 * nor the saved report's text box has to break one in the middle of a word.
 * Shown on the error scene under the heading; the background behind it is in
 * docs/troubleshooting.md.
 */
const char* emrtd_error_hint(EmrtdError error);

/** Map a status word to the closest error. */
EmrtdError emrtd_error_from_sw(uint16_t sw);

/** Human readable meaning of a status word, for the trace and the UI. */
const char* emrtd_sw_text(uint16_t sw);

#ifdef __cplusplus
}
#endif
