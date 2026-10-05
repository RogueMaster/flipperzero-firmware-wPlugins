/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The text behind every error code.
 *
 * The person holding the document sees a heading and a hint, and they are the
 * only explanation they get: there is no log to consult and no second screen.
 * So the hint names the next thing to try, and where the failure is a
 * property of this hardware rather than of the document it says so, because
 * otherwise the user keeps retrying something that cannot work. The background
 * behind each one is in docs/troubleshooting.md, not here.
 *
 * Both have to fit a 128x64 screen, so the budgets are pixels of the
 * firmware's own fonts, and tests/host measures every string against them:
 *
 * - The heading is a single FontPrimary string element, and that does not
 *   wrap: whatever is wider than the screen is cut off at both edges. At most
 *   124 px of glyph advances, which is about twenty characters.
 * - The hint goes into a text scroll element, which breaks a line at whichever
 *   glyph crosses its width, in the middle of a word if need be. So every hint
 *   is broken by hand, one string literal to a screen line, and each line stays
 *   within 120 px of FontSecondary: 124 px is the scroll element, 120 px the
 *   text box that shows the same hint again in a saved report.
 * - Three lines at most. That is what the error screen shows above its buttons
 *   without scrolling, and it is all most people will read.
 *
 * A hint is only ever read for an error that ended the whole read - on the
 * error screen and in report.txt. A file that fails is marked with its heading
 * and the read goes on, so what a hint describes is the application, the key
 * exchange or the radio, never one file.
 */

#include "emrtd_error.h"

typedef struct {
    const char* text;
    const char* hint;
} EmrtdErrorStrings;

/*
 * Deliberately left unsized: the static assertion below then fails if an
 * enumerator is added without a line of text, instead of silently yielding a
 * NULL entry. The designated indices keep the table in step with the enum
 * even when the order changes.
 */
static const EmrtdErrorStrings emrtd_error_strings[] = {
    [EmrtdErrorNone] =
        {"No error",
         "The read finished\n"
         "without a problem."},

    [EmrtdErrorNoCard] =
        {"No document found",
         "Lay the Flipper flat on the\n"
         "data page or the card and\n"
         "hold still. Try the cover too."},

    [EmrtdErrorCardLost] =
        {"Document moved away",
         "The chip left the field.\n"
         "Put it back and hold both\n"
         "still until the bar fills."},

    [EmrtdErrorActivation] =
        {"Chip would not connect",
         "It answered, then would not\n"
         "start a session. Lift the\n"
         "Flipper, lay it back, retry."},

    [EmrtdErrorTransport] =
        {"Radio exchange failed",
         "A reply did not arrive.\n"
         "Move phones, metal and\n"
         "other cards away, retry."},

    [EmrtdErrorProtocol] =
        {"Garbled chip reply",
         "Usually a weak field, not\n"
         "a faulty chip. Move the\n"
         "document a little, retry."},

    [EmrtdErrorNotEmrtd] =
        {"Not an eMRTD document",
         "This chip has no eMRTD\n"
         "application. Bank, transit\n"
         "and access cards never do."},

    [EmrtdErrorApdu] =
        {"Chip refused command",
         "It sent an error status\n"
         "instead of data. An APDU\n"
         "trace shows which one."},

    [EmrtdErrorFileNotFound] =
        {"Not found on this chip",
         "The chip would not open\n"
         "the eMRTD application.\n"
         "Lay it back on and retry."},

    /*
     * What reaches this screen is a refusal of the application or of the key
     * exchange, not of a file: a file that is refused is marked and the read
     * goes on, and DG3 and DG4 are never asked for. 6983 means the chip has
     * blocked itself after too many failures, so the hint says when to stop.
     */
    [EmrtdErrorAccessDenied] =
        {"Chip refused access",
         "Lay it back on and retry.\n"
         "If it keeps refusing, stop:\n"
         "the chip may lock itself."},

    [EmrtdErrorWrongKey] =
        {"Key not accepted",
         "Check the CAN, or number\n"
         "and dates. Type the number\n"
         "without its check digit."},

    [EmrtdErrorNoAccessMethod] =
        {"No way into the chip",
         "This chip announces no\n"
         "PACE. Set Options > Access\n"
         "method to Automatic."},

    [EmrtdErrorPaceUnsupportedCurve] =
        {"PACE curve not usable",
         "Unknown, or over 256 bits\n"
         "for mbed TLS. BAC needs\n"
         "the number and dates."},

    [EmrtdErrorPaceUnsupportedMapping] =
        {"PACE mapping missing",
         "Only generic mapping is\n"
         "built in. BAC needs the\n"
         "number and dates."},

    [EmrtdErrorPaceUnsupportedDh] =
        {"PACE DH not supported",
         "The Flipper's mbed TLS has\n"
         "no DH, only curves. BAC\n"
         "needs the number and dates."},

    [EmrtdErrorPaceFailed] =
        {"PACE key rejected",
         "The CAN or the MRZ values\n"
         "do not match. The CAN is a\n"
         "separate 6 digit number."},

    [EmrtdErrorSecureMessaging] =
        {"Secure channel broke",
         "A reply failed its check\n"
         "and was dropped. Move the\n"
         "document a little, retry."},

    [EmrtdErrorParse] =
        {"Malformed data",
         "A chip reply does not\n"
         "match ICAO 9303. Retry, or\n"
         "send an APDU trace."},

    [EmrtdErrorUnsupported] =
        {"Not supported",
         "This chip's PACE variant\n"
         "is not built in. BAC needs\n"
         "the number and dates."},

    [EmrtdErrorOutOfMemory] =
        {"Not enough memory",
         "Close qFlipper or\n"
         "lab.flipper.net, unplug\n"
         "USB, restart the Flipper."},

    [EmrtdErrorStorage] =
        {"SD card write failed",
         "Check the card is in, not\n"
         "locked and not full. The\n"
         "read itself worked."},

    [EmrtdErrorBufferTooSmall] =
        {"Value too large",
         "A reply is bigger than\n"
         "this reader expects. An\n"
         "APDU trace shows which."},

    [EmrtdErrorInvalidInput] =
        {"Details incomplete",
         "Set the number and both\n"
         "dates, or a CAN. Press\n"
         "Document to fill them in."},

    [EmrtdErrorCancelled] =
        {"Read cancelled",
         "You left before it ended.\n"
         "Files read until then are\n"
         "kept in this folder."},

    [EmrtdErrorInternal] =
        {"Internal error",
         "Should not happen. Restart\n"
         "the app; if it repeats,\n"
         "report it with a trace."},
};

_Static_assert(
    sizeof(emrtd_error_strings) / sizeof(emrtd_error_strings[0]) == EmrtdErrorCount,
    "every EmrtdError needs a line of text and a hint");

static const EmrtdErrorStrings* emrtd_error_lookup(EmrtdError error) {
    if((size_t)error >= (size_t)EmrtdErrorCount) {
        return NULL;
    }
    const EmrtdErrorStrings* entry = &emrtd_error_strings[error];
    /* A hole left by a future enumerator reads as NULL rather than crashing. */
    return entry->text != NULL ? entry : NULL;
}

const char* emrtd_error_text(EmrtdError error) {
    const EmrtdErrorStrings* entry = emrtd_error_lookup(error);
    return entry != NULL ? entry->text : "Unknown error";
}

const char* emrtd_error_hint(EmrtdError error) {
    const EmrtdErrorStrings* entry = emrtd_error_lookup(error);
    return entry != NULL ? entry->hint :
                           "This error has no text,\n"
                           "which is a bug. Please\n"
                           "report what you read.";
}

EmrtdError emrtd_error_from_sw(uint16_t sw) {
    switch(sw) {
    case 0x9000:
        return EmrtdErrorNone;

    /* ISO 7816-4 table 6: the file or the application is simply not there. */
    case 0x6A82:
    case 0x6A83:
        return EmrtdErrorFileNotFound;

    /*
     * 6982 is the ordinary "you have not authenticated" and 6983 is a chip
     * that has locked itself after repeated failures; both are refusals of
     * the request rather than of the key.
     */
    case 0x6982:
    case 0x6983:
        return EmrtdErrorAccessDenied;

    /*
     * 6300 is what a chip returns from EXTERNAL AUTHENTICATE when the BAC
     * response did not verify, which means the MRZ input was wrong.
     */
    case 0x6300:
        return EmrtdErrorWrongKey;

    /* 6987 and 6988 are about the Secure Messaging data objects themselves. */
    case 0x6987:
    case 0x6988:
        return EmrtdErrorSecureMessaging;

    default:
        break;
    }

    /* 63Cx counts down the remaining attempts; the key was still wrong. */
    if((sw & 0xFFF0) == 0x63C0) {
        return EmrtdErrorWrongKey;
    }

    return EmrtdErrorApdu;
}

const char* emrtd_sw_text(uint16_t sw) {
    switch(sw) {
    case 0x9000:
        return "OK";
    case 0x6200:
        return "warning, no information";
    case 0x6281:
        return "part of the data may be corrupted";
    case 0x6282:
        return "end of file reached before Le bytes";
    case 0x6300:
        return "verification failed";
    case 0x6400:
        return "execution error, state unchanged";
    case 0x6700:
        return "wrong length";
    case 0x6800:
        return "class byte not supported";
    case 0x6882:
        return "secure messaging not supported";
    case 0x6883:
        return "the last command of the chain is missing";
    case 0x6900:
        return "command not allowed";
    case 0x6982:
        return "security status not satisfied";
    case 0x6983:
        return "authentication method blocked";
    case 0x6984:
        return "reference data unusable";
    case 0x6985:
        return "conditions of use not satisfied";
    case 0x6986:
        return "no current elementary file";
    case 0x6987:
        return "expected secure messaging objects missing";
    case 0x6988:
        return "secure messaging objects incorrect";
    case 0x6A80:
        return "incorrect parameters in the data field";
    case 0x6A81:
        return "function not supported";
    case 0x6A82:
        return "file or application not found";
    case 0x6A83:
        return "record not found";
    case 0x6A86:
        return "incorrect parameters P1-P2";
    case 0x6A88:
        return "referenced data not found";
    case 0x6B00:
        return "wrong parameters, offset outside the file";
    case 0x6D00:
        return "instruction not supported";
    case 0x6E00:
        return "class not supported";
    case 0x6F00:
        return "no precise diagnosis";
    default:
        break;
    }

    /*
     * The three status word families. They carry a count in the low byte that
     * the caller already has, so the text stays a constant and this function
     * keeps no state of its own - it is called from the NFC thread.
     */
    if((sw & 0xFF00) == 0x6100) {
        return "more data available, issue GET RESPONSE";
    }
    if((sw & 0xFF00) == 0x6C00) {
        return "wrong Le, the low byte is the correct length";
    }
    if((sw & 0xFFF0) == 0x63C0) {
        return "verification failed, the low nibble counts the attempts left";
    }

    return "unknown status word";
}
