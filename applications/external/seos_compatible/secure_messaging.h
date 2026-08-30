#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <mbedtls/des.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include "seos_common.h"
#include "cmac.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scratch for wrapping and unwrapping is sized from the message, so there is
 * no fixed ceiling here and no per-call stack cost for the largest one.
 *
 * A command still states its own length in a single byte, so the objects and
 * checksum of one command have to fit in 255. Anything longer has to be
 * carried in pieces by the transport. */

/* Largest message a single command can carry, for either cipher. Wrapping
 * works the exact limit out from the block size and refuses anything past it;
 * this is the smaller of the two, and safe for both. */
#define SECURE_MESSAGING_COMMAND_MAX 223

/* The command header covered by the checksum. */
#define SECURE_MESSAGING_APDU_HEADER_LEN 4

/* Secure messaging errors are answered unprotected, and end the session. */
#define SECURE_MESSAGING_SW_MISSING_DO   0x6987
#define SECURE_MESSAGING_SW_INCORRECT_DO 0x6988

/* Where the cryptogram starts. A command carries a four byte header and a
 * length byte ahead of it; a response body starts with the cryptogram. */
#define SECURE_MESSAGING_CAPDU_BODY_OFFSET 5
#define SECURE_MESSAGING_RAPDU_BODY_OFFSET 0

typedef struct {
    uint8_t cipher;
    uint8_t PrivacyKey[16];
    uint8_t CMACKey[16];
    uint8_t aesContext[16];
    uint8_t desContext[8];
    /* Status word for the last message that failed to unwrap, or 0. */
    uint16_t last_error_sw;
    /* Protected status word of the last response unwrapped, or 0. */
    uint16_t last_response_sw;

    /* Remainder of a response too long for one frame, waiting to be asked
     * for. Allocated only when a response actually needs chaining. */
    uint8_t* pending;
    size_t pending_len;
    size_t pending_offset;
} SecureMessaging;

SecureMessaging* secure_messaging_alloc(AuthParameters* params);

void secure_messaging_free(SecureMessaging* secure_messaging);

/* Holds a response that will be handed out a frame at a time. Replaces any
 * response already pending. */
bool secure_messaging_set_pending(
    SecureMessaging* secure_messaging,
    const uint8_t* data,
    size_t len);

/* Copies the next piece of a pending response into `out`, at most `max_len`
 * bytes, and reports how much is left after it. */
size_t secure_messaging_take_pending(
    SecureMessaging* secure_messaging,
    uint8_t* out,
    size_t max_len,
    size_t* remaining);

void secure_messaging_clear_pending(SecureMessaging* secure_messaging);

void secure_messaging_increment_context(SecureMessaging* secure_messaging);

/* The wrap calls return false if the message will not fit or the cipher
 * refuses it, leaving the output buffer alone.
 *
 * The unwrap calls replace the buffer contents with the recovered plaintext
 * and return true. They return false, leaving the buffer untouched, if the
 * message is malformed or the padding is wrong -- a caller must check before
 * reading what it thinks is plaintext. */
/* `expects_response` says whether the plain command carried an Le.
 *
 * A command that expects data back carries a protected Le object and covers it
 * with the checksum; one that does not, does neither. A card that follows the
 * standard builds its checksum over the objects it received, so sending the
 * object where it does not belong makes the two disagree. */
bool secure_messaging_wrap_apdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint8_t* apdu_header,
    size_t apdu_header_len,
    bool expects_response,
    BitBuffer* tx_buffer);

bool secure_messaging_unwrap_apdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);

bool secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);
/* Wraps a response. `status_word` is carried in the protected status object
 * and must match the one the caller sends in the clear. A response with no
 * data omits the cryptogram rather than encrypting nothing. */
bool secure_messaging_wrap_rapdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint16_t status_word,
    BitBuffer* tx_buffer);

#ifdef __cplusplus
}
#endif
