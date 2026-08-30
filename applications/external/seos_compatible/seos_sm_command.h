#pragma once

#include "seos_credential.h"
#include "secure_messaging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Handling for commands that arrive inside a secure messaging session.
 *
 * Every transport carried its own copy of this, so a fix had to be made four
 * times and the copies had already drifted. There is one copy now, and it
 * reports what happened through a callback rather than reaching into the UI.
 */

/* Command headers a secure message carries: reading an object, and writing
 * one. Both address the current file. */
extern const uint8_t SEOS_SM_HEADER[4];
extern const uint8_t SEOS_SM_PUT_HEADER[4];

/* The command asking for the next piece of a chained response. The last byte
 * is Le, which the caller sets from the count the card reported. */
#define SEOS_GET_RESPONSE_LEN 5
extern const uint8_t SEOS_GET_RESPONSE[SEOS_GET_RESPONSE_LEN];

/* Largest response the handler will assemble before splitting it. */
#define SEOS_SM_RESPONSE_MAX 256

/* Tags one request may name. A reader asking for more than a handful at once
 * is asking for objects this card does not hold. */
#define SEOS_SM_MAX_REQUESTED_TAGS 8

/* Bytes a single frame carries, status word included. Every transport here
 * works to the same budget; anything longer is chained. */
#define SEOS_SM_MAX_FRAME 128

/* Frames one chained response may take. A bound on a loop driven by the other
 * end, so a card that never says it is finished cannot hold the reader. */
#define SEOS_SM_MAX_CHAINED_FRAMES 8

typedef enum {
    SeosSmEventSioRequested,
    SeosSmEventSioWritten,
} SeosSmEvent;

typedef void (*SeosSmEventCallback)(void* context, SeosSmEvent event);

/* Handles one wrapped command.
 *
 * `apdu` points at the command with any transport framing already stripped.
 * The wrapped response is appended to `tx`, status word included.
 *
 * Returns false when the session is finished -- a secure messaging error is
 * answered in the clear and the counters are no longer in step, so the caller
 * should drop the session.
 */
bool seos_sm_command_handle(
    SecureMessaging* secure_messaging,
    SeosCredential* credential,
    const uint8_t* apdu,
    size_t apdu_len,
    size_t max_frame_len,
    BitBuffer* tx,
    SeosSmEventCallback on_event,
    void* event_context);

/* Hands out the next piece of a response that did not fit one frame.
 *
 * `le` is the length the reader asked for, with zero meaning 256. The answer
 * is at most that much, and at most what the frame holds.
 *
 * Answers 6a86 if nothing is pending, since the reader asked for a
 * continuation that does not exist. */
void seos_sm_command_get_response(
    SecureMessaging* secure_messaging,
    size_t max_frame_len,
    uint8_t le,
    BitBuffer* tx);

/* What to do with the status word a card just answered with.
 *
 * Kept apart from the transport so the rule can be read and tested on its own:
 * the reader's exchange loop only has to act on the answer. */
typedef enum {
    SeosExchangeDone, /* the response is complete */
    SeosExchangeContinue, /* more is waiting; ask for `expected_length` of it */
    SeosExchangeResend, /* the card wants a different expected length */
    SeosExchangeFailed,
} SeosExchangeStep;

/* `already_resent` stops a card that keeps asking for a different length from
 * holding the reader in a loop. */
SeosExchangeStep
    seos_sm_next_step(uint8_t sw1, uint8_t sw2, bool already_resent, uint8_t* expected_length);

/* Whether an APDU is a command this handler serves. */
bool seos_sm_command_matches(const uint8_t* apdu, size_t apdu_len);

/* Appends a status word to a response, most significant byte first. */
void seos_sm_append_status(BitBuffer* tx, uint16_t status_word);

#ifdef __cplusplus
}
#endif
