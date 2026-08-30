#pragma once

#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "seos_credential.h"
#include "secure_messaging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The Seos exchange itself: selecting an application, agreeing a session key,
 * and the cryptograms on either side of it.
 *
 * This is transport neutral. The NFC poller and listener drive it, and so do
 * the BLE stacks, which is why it does not live with either of them.
 */

/* Status words the exchange answers with. */
#define SEOS_SW_SUCCESS_VALUE   0x9000
#define SEOS_SW_WRONG_DATA      0x6a80
#define SEOS_SW_WRONG_P1P2      0x6a86
#define SEOS_SW_NOT_ENOUGH_ROOM 0x6a84

extern const uint8_t SEOS_SW_SUCCESS[2];
extern const uint8_t SEOS_SW_FILE_NOT_FOUND[2];

/* The authenticate command, whichever keyset it names.
 *
 * The reference data qualifier in P2 selects the keyset, so it is not part of
 * the match; matching it would answer only one keyset and silently ignore
 * every other. */
bool seos_is_general_authenticate_1(const uint8_t* apdu, size_t apdu_len);
bool seos_is_general_authenticate_2(const uint8_t* apdu, size_t apdu_len);

/* Length of the first authenticate command. */
#define SEOS_GENERAL_AUTHENTICATE_1_LEN 10

/* Builds the first authenticate command for a given keyset. */
void seos_build_general_authenticate_1(
    uint8_t key_no,
    uint8_t out[SEOS_GENERAL_AUTHENTICATE_1_LEN]);

/* The application identifier a SELECT names.
 *
 * `aid` points into `apdu`, and its length is the one the command states,
 * checked against what the command actually carries. Callers compare the
 * identifier against the ones they serve; a length that does not match one of
 * those is not that application. */
bool seos_parse_select_aid(
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t** aid,
    size_t* aid_len);

/* The application list a SELECT ADF names.
 *
 * `oid_list` points into `apdu`. The command states its own length, which comes
 * from the reader, so it is checked against what the APDU actually holds before
 * anything walks it. False if the header does not match, the command is shorter
 * than it claims, or it names nothing. */
bool seos_parse_select_adf(
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t** oid_list,
    size_t* oid_list_len);

/* Answers that give nothing away.
 *
 * A card that returns an error when it does not hold what was asked for, or
 * when authentication fails, tells anyone who asks what it carries and
 * whether their key was right. These answer with well formed nonsense and a
 * success word instead, which is indistinguishable without the key. */
void seos_emulator_shill_select_adf(BitBuffer* tx_buffer);
void seos_emulator_shill_authenticate(BitBuffer* tx_buffer);

/* Card side. */
void seos_emulator_select_aid(BitBuffer* tx_buffer, const uint8_t* aid, size_t aid_len);

bool seos_emulator_select_adf(
    const uint8_t* oid_list,
    size_t oid_list_len,
    AuthParameters* params,
    SeosCredential* credential,
    BitBuffer* tx_buffer);

void seos_emulator_general_authenticate_1(BitBuffer* tx_buffer, AuthParameters params);

bool seos_emulator_general_authenticate_2(
    const uint8_t* buffer,
    size_t buffer_len,
    SeosCredential* credential,
    AuthParameters* params,
    BitBuffer* tx_buffer);

/* Reader side.
 *
 * The answers a card gives, read from a buffer and a length. Kept apart from
 * the poller so the shape of a response can be checked without a card, and so
 * a malformed one is refused in one place rather than at each call site. */

/* The status word a response ends with. False if it is too short to carry
 * one, which is the case a step back from the end would otherwise miss. */
bool seos_response_status(const uint8_t* data, size_t len, uint16_t* status_word);

/* The card's challenge, from the answer to the first authenticate command. */
bool seos_parse_ga1_response(const uint8_t* data, size_t len, uint8_t* rnd_icc, size_t rnd_icc_len);

/* The card's cryptogram, from the answer to the second. `cryptogram` points
 * into `data`, and its length is reported rather than assumed: the caller
 * decides which lengths it can verify. */
bool seos_parse_ga2_response(
    const uint8_t* data,
    size_t len,
    const uint8_t** cryptogram,
    size_t* cryptogram_len);

/* The credential a card holds, named by its file identifier. */
#define SEOS_SIO_FILE_TAG 0xff00

/* Whether a card's answer says it stored what a write sent it.
 *
 * The answer is protected, so the status word in the clear settles nothing on
 * its own. This verifies the checksum over the protected status, and steps the
 * session counter for the response, which the next command depends on.
 *
 * `rx_buffer` is left holding whatever plaintext the answer carried. */
bool seos_reader_write_accepted(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);

/* The credential, from an unwrapped read answer. */
bool seos_parse_sio_response(
    const uint8_t* data,
    size_t len,
    uint8_t* sio,
    size_t sio_cap,
    size_t* sio_len);

bool seos_reader_select_adf_response(
    BitBuffer* rx_buffer,
    size_t offset,
    SeosCredential* credential,
    AuthParameters* params);

void seos_reader_generate_cryptogram(
    SeosCredential* credential,
    AuthParameters* params,
    uint8_t* cryptogram);

/* Length of the cryptogram a card answers the second authenticate with. */
#define SEOS_CARD_CRYPTOGRAM_LEN 40

bool seos_reader_verify_cryptogram(AuthParameters* params, const uint8_t* cryptogram);

#ifdef __cplusplus
}
#endif
