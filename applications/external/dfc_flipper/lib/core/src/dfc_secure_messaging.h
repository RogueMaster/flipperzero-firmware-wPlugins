#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "dfc_common.h"

#define DFC_SM_MAX_SIZE 192

typedef struct {
    uint8_t cipher; // DFC_CMD_AUTHENTICATE_LEGACY / _ISO / _AES
    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len;
    // Shared CBC IV, chained across every enciphered-mode operation regardless of
    // direction, for ISO(0x1A)/AES(0xAA) sessions. The IV for each new CBC operation is
    // the last ciphertext block of the preceding operation. Legacy (0x0A) sessions use
    // an all-zero IV for each operation.
    uint8_t iv[16];

    // True when this session belongs to the reader. A legacy (D40) session's
    // enciphered mode picks its DES primitive by party rather than by direction,
    // so the two ends are not interchangeable there. Unset means PICC.
    bool pcd;

    // Reusable buffers to keep them off the worker stack
    uint8_t crypto_scratch[DFC_SM_MAX_SIZE];
    uint8_t mac_input_scratch[DFC_SM_MAX_SIZE];
} DfcSecureMessaging;

// `initial_iv` seeds the chained IV for ISO/AES sessions (pass the final chained IV state
// from the authentication handshake); ignored for legacy sessions. Pass NULL to start from
// all-zero (e.g. legacy, or tests that don't care about IV chaining).
DfcSecureMessaging* dfc_secure_messaging_alloc(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv);

void dfc_secure_messaging_free(DfcSecureMessaging* sm);
void dfc_secure_messaging_reset_iv(DfcSecureMessaging* sm);
bool dfc_secure_messaging_applies_ev1(DfcSecureMessaging* sm, uint8_t cmd);
void dfc_secure_messaging_update_ev1_command(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len);
// EV1 option-b commands (WriteData, Credit, …): verify trailing 8-byte truncated
// CMAC over Cmd||data_without_mac, update IV from full CMAC. Returns clear length
// (data_len - 8) or SIZE_MAX on failure.
size_t dfc_secure_messaging_verify_ev1_transmitted_command_mac(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len);
// True for WriteData / Credit / Debit / LimitedCredit / WriteRecord / UpdateRecord
// (EV1 MAC-mode commands that place MACt on the wire).
bool dfc_secure_messaging_ev1_transmits_command_mac(uint8_t cmd);
size_t dfc_secure_messaging_generate_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out);
// Returns the cleartext length, or SIZE_MAX on CMAC/format failure.
size_t dfc_secure_messaging_unwrap_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out);

// Wraps a command payload under the given file's communication mode
// (plain/MAC/enciphered). `header` is the cleartext head of the command - the
// command byte followed by the parameters that precede the payload, so
// Cmd||FileNo||Offset||Length for a write - and `plain` is the payload after it.
// An EV1 session's MAC and CRC cover header and payload together; a legacy
// session covers the payload alone. Writes the wrapped payload (never the
// header) into `out`.
size_t dfc_secure_messaging_wrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out);

// Unwraps a response payload (status byte NOT included) received under the given
// communication mode. Returns the cleartext length, or 0 on MAC/format failure.
size_t dfc_secure_messaging_unwrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out);

// Emulator (PICC) side: generates a response payload under the given comm mode, matching
// what dfc_secure_messaging_unwrap() on the reader side will accept.
size_t dfc_secure_messaging_generate_response(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out);

// Emulator (PICC) side: verifies/unwraps an incoming command payload under the given comm
// mode, matching what dfc_secure_messaging_wrap() on the reader side produced.
size_t dfc_secure_messaging_verify_command(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out);
