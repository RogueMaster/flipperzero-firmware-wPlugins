#pragma once

// Reader (PCD) side of the DESFire protocol.
//
// The reader never touches a transport. Every operation is a small state
// machine: dfc_reader_step hands back the next frame to send, the caller sends
// it however it likes, and passes the card's answer to the next step. That runs
// the same on a microcontroller driving an NFC front end, on a workstation
// talking PC/SC, and against an in-process virtual card:
//
//     DfcReaderExchange ex;
//     dfc_reader_exchange_begin(&ex, &session, DfcReaderFramingIso7816, &command, NULL);
//     const uint8_t* rx = NULL;
//     size_t rx_len = 0;
//     DfcReaderStatus st;
//     while((st = dfc_reader_step(&ex, rx, rx_len, tx, sizeof(tx), &tx_len)) ==
//           DfcReaderPending) {
//         transmit(tx, tx_len, rx_buffer, &rx_len);
//         rx = rx_buffer;
//     }
//
// The reader never draws randomness either. Authentication takes the reader's
// challenge as an argument, so the caller decides where it comes from and a
// test can replay a recorded session exactly.
//
// A session holds the keys and chaining state one authentication establishes.
// Each exchange run under it advances that state the way the card does, so the
// two stay in step for as long as the card keeps the session open.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_command.h"
#include "dfc_secure_messaging.h"

#if DFC_ENABLE_READER

// Largest response, after reassembling every additional frame, one exchange
// collects.
#ifndef DFC_READER_MAX_RESPONSE
#define DFC_READER_MAX_RESPONSE 600
#endif
// Largest secured command field, before an EV2 exchange splits it into frames.
#ifndef DFC_READER_MAX_COMMAND
#define DFC_READER_MAX_COMMAND 520
#endif

// Authentication a session holds. The legacy, ISO and AES values are the
// instruction octets that start each, as elsewhere in the library.
#define DFC_READER_AUTH_NONE 0x00
#define DFC_READER_AUTH_EV2  DFC_CMD_AUTHENTICATE_EV2_FIRST

typedef enum {
    DfcReaderOk = 0,
    // A frame is ready to send. Pass the card's answer to the next step.
    DfcReaderPending,
    // An argument is out of range, or the operation does not fit the session.
    DfcReaderInvalid,
    // A caller buffer, or one of the reader's own bounds, is too small.
    DfcReaderBufferTooSmall,
    // The card's answer is malformed or not the answer this step expects.
    DfcReaderProtocolError,
    // A MAC, a CRC or a returned challenge did not verify.
    DfcReaderIntegrityError,
    // The card answered with an error status; dfc_reader_result_status has it.
    DfcReaderCardError,
    // This build does not carry what the operation needs.
    DfcReaderUnsupported,
} DfcReaderStatus;

// How frames travel. ISO 7816 wraps each native command as 90 INS 00 00 Lc
// data 00 and the card answers data 91 status. Native framing sends INS data
// and the card answers status data.
typedef enum {
    DfcReaderFramingIso7816 = 0,
    DfcReaderFramingNative,
} DfcReaderFraming;

typedef struct {
    // DFC_READER_AUTH_NONE, DFC_CMD_AUTHENTICATE_LEGACY / _ISO / _AES, or
    // DFC_READER_AUTH_EV2.
    uint8_t auth_mode;
    uint8_t key_no;
    // Legacy and EV1 session key and chaining IV. `pcd` is always set.
    DfcSecureMessaging sm;
    uint8_t ev2_transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint16_t ev2_command_counter;
    uint8_t ev2_encryption_key[DFC_AES_KEY_LENGTH];
    uint8_t ev2_mac_key[DFC_AES_KEY_LENGTH];
    uint8_t ev2_card_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t ev2_reader_capabilities[DFC_EV2_CAPABILITY_LENGTH];
} DfcReaderSession;

typedef struct {
    // DFC_COMM_PLAIN, DFC_COMM_MAC or DFC_COMM_ENCIPHERED: the communication
    // mode of the file the command addresses. A zeroed options value is plain.
    uint8_t comm_mode;
    // Octets after the instruction that stay clear when the data field is
    // encrypted. Unset, it is derived from the instruction.
    bool has_header_len;
    uint8_t header_len;
} DfcReaderOptions;

typedef struct {
    const uint8_t* data;
    size_t length;
    size_t offset;
    uint32_t base_offset;
    uint32_t record_number;
    uint8_t file_number;
    uint8_t comm_mode;
    uint8_t kind;
} DfcReaderStream;

// One operation in flight. Treat the fields as private.
typedef struct {
    DfcReaderSession* session;
    DfcReaderFraming framing;
    uint8_t kind;
    uint8_t phase;
    uint8_t ins;
    uint8_t comm_mode;
    uint8_t response_plan;
    bool invalidates_session;
    bool special_success;

    uint8_t frame[DFC_READER_MAX_COMMAND];
    size_t frame_len;
    size_t frame_offset;
    // When set, the first frame carries only this many octets.
    size_t first_frame_len;
    // When set, every continuation carries at most this many octets.
    size_t command_frame_len;
    DfcReaderStream stream;
    // Clear command head for an EV1 encrypted read's CRC, kept per exchange.
    uint8_t command_head[16];
    size_t command_head_len;

    uint8_t data[DFC_READER_MAX_RESPONSE];
    size_t data_len;
    uint8_t status;

    // Authentication in progress.
    uint8_t auth_cipher;
    uint8_t auth_key_no;
    uint8_t auth_key[DFC_MAX_KEY_LEN];
    size_t auth_key_len;
    size_t auth_challenge_len;
    uint8_t auth_random_a[DFC_EV2_RANDOM_LENGTH];
    uint8_t auth_random_b[DFC_EV2_RANDOM_LENGTH];
#if DFC_ENABLE_ISO7816_AUTH
    uint8_t auth_random_second[DFC_ISO7816_AUTH_CHALLENGE_LONG];
    uint8_t auth_reference;
    uint8_t auth_algorithm;
#endif
    uint8_t auth_iv[16];
    bool auth_first;
    uint8_t auth_capabilities[DFC_EV2_CAPABILITY_LENGTH];
} DfcReaderExchange;

const char* dfc_reader_status_name(DfcReaderStatus status);

// An unauthenticated session.
void dfc_reader_session_init(DfcReaderSession* session);
// Forget the session's keys, as the card does when it ends a session.
void dfc_reader_session_clear(DfcReaderSession* session);
bool dfc_reader_session_is_authenticated(const DfcReaderSession* session);

// Challenge length the reader supplies for one authentication: 16 octets for
// AES and for a 24-octet (3K3DES) key, 8 otherwise.
size_t dfc_reader_challenge_length(uint8_t cipher, size_t key_len);

// Legacy (D40), ISO or AES three-pass authentication. `random_a` must hold
// dfc_reader_challenge_length octets. The session is replaced when the card
// accepts, and cleared when it refuses.
DfcReaderStatus dfc_reader_authenticate_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t cipher,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* random_a,
    size_t random_a_len);

#if DFC_ENABLE_ISO7816_AUTH
// Standard ISO 7816 mutual authentication uses two caller-supplied challenges.
// The key reference is 0x00-0x0D at PICC level or 0x80-0x8D in an application.
DfcReaderStatus dfc_reader_authenticate_iso7816_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    uint8_t key_reference,
    const uint8_t* key,
    size_t key_len,
    uint8_t algorithm,
    const uint8_t* random_first,
    const uint8_t* random_second,
    size_t random_len);
#endif

// EV2 authentication with a 16-octet AES key. The first authentication opens a
// transaction and may carry six octets of reader capabilities; a non-first one
// keeps the transaction of the EV2 session already held.
DfcReaderStatus dfc_reader_authenticate_ev2_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    bool first,
    uint8_t key_no,
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t* capabilities,
    size_t capabilities_len);

// One command under the session's secure messaging, following every additional
// frame. `options` may be NULL for a plain command.
DfcReaderStatus dfc_reader_exchange_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    const DfcCommand* command,
    const DfcReaderOptions* options);

// Write a caller-owned payload in bounded commands. The caller must keep
// `data` valid until dfc_reader_step stops returning DfcReaderPending. Record
// writes reuse the same uncommitted record; the caller commits or aborts it.
DfcReaderStatus dfc_reader_write_data_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);
DfcReaderStatus dfc_reader_write_record_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);
DfcReaderStatus dfc_reader_update_record_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);

// Advance an operation. Pass NULL and 0 on the first call, then each answer.
// Returns DfcReaderPending with the next frame in `out`, DfcReaderOk when the
// operation is complete, or the reason it failed.
DfcReaderStatus dfc_reader_step(
    DfcReaderExchange* exchange,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

// The card's final status and its clear response data, after the MAC is
// stripped and anything encrypted is decrypted.
uint8_t dfc_reader_result_status(const DfcReaderExchange* exchange);
const uint8_t* dfc_reader_result_data(const DfcReaderExchange* exchange, size_t* len);

// The ChangeKey cryptogram for a legacy, ISO or AES session: the data field
// after the key number. `current_key` is required when changing a key other
// than the authenticated one. `new_version` is carried for AES keys only.
DfcReaderStatus dfc_reader_change_key_cryptogram(
    const DfcReaderSession* session,
    uint8_t key_no,
    const uint8_t* new_key,
    size_t key_len,
    const uint8_t* current_key,
    bool aes_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

#if DFC_ENABLE_EV2_SECURE_MESSAGING
// ChangeKeyEV2 for an AES key set, as a command to run with
// dfc_reader_exchange_begin, which encrypts its data field. `current_key` is
// required unless the key is the authenticated one in key set 0; changing that
// one ends the session.
DfcReaderStatus dfc_reader_change_key_ev2_command(
    const DfcReaderSession* session,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t new_key[DFC_AES_KEY_LENGTH],
    const uint8_t* current_key,
    uint8_t new_version,
    DfcCommand* command);

// CreateDelegatedApplication under an EV2 session opened with the DAM
// authentication key. The initial key is encrypted under the DAM encryption
// key and MACed under the DAM MAC key; `random_prefix` pads it. Run it with
// dfc_reader_step.
DfcReaderStatus dfc_reader_create_delegated_application_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    const DfcCommandCreateDelegatedApplication* app,
    const uint8_t dam_encryption_key[DFC_AES_KEY_LENGTH],
    const uint8_t dam_mac_key[DFC_AES_KEY_LENGTH],
    const uint8_t random_prefix[DFC_DELEGATED_RANDOM_PREFIX_LENGTH],
    const uint8_t* initial_key,
    size_t initial_key_len,
    uint8_t initial_version);
#endif

// The proximity-check MAC over the parameters PrepareProximityCheck published
// and the transcript of every round (the card's octets, then the reader's, per
// round). `from_card` selects the card's answer to VerifyProximityCheck rather
// than the reader's command.
bool dfc_reader_proximity_check_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    bool from_card,
    const uint8_t* published,
    size_t published_len,
    const uint8_t* transcript,
    size_t transcript_len,
    uint8_t mac[DFC_WIRE_MAC_LENGTH]);

// Virtual-card selection: an ISO SELECT by installation identifier, the
// decrypted challenge and clear data from the card's answer, and the EXTERNAL
// AUTHENTICATE that proves the reader holds the select MAC key.
DfcReaderStatus dfc_reader_virtual_card_select_apdu(
    const uint8_t* installation_id,
    size_t installation_id_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);
DfcReaderStatus dfc_reader_virtual_card_open(
    const uint8_t select_encryption_key[DFC_AES_KEY_LENGTH],
    const uint8_t* response,
    size_t response_len,
    uint8_t challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH],
    uint8_t clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH]);
DfcReaderStatus dfc_reader_virtual_card_authenticate_apdu(
    const uint8_t select_mac_key[DFC_AES_KEY_LENGTH],
    const uint8_t challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH],
    const uint8_t clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH],
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

#endif // DFC_ENABLE_READER
