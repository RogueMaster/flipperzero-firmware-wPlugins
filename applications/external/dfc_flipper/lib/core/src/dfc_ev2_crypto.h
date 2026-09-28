#pragma once

// EV2 secure-messaging arithmetic shared by both parties.
//
// Nothing here knows which end of the link it runs on: every function takes the
// session values it needs, so the emulator and the reader derive, MAC and
// encrypt through the same code and cannot disagree about the wire.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_common.h"

#if DFC_ENABLE_EV2_SECURE_MESSAGING

// Header of every EV2 MAC input: one octet (instruction or status), the
// command counter and the transaction identifier.
#define DFC_EV2_MAC_PREFIX_LENGTH \
    (1 + DFC_EV2_COUNTER_LENGTH + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH)
#define DFC_EV2_PADDING_MARKER 0x80

// Largest body one EV2 MAC covers. A chained command is MACed as a whole, so
// this is sized above one frame.
#ifndef DFC_EV2_MAC_DATA_MAX
#define DFC_EV2_MAC_DATA_MAX 512
#endif

// Which direction a data field travels, selecting the IV label.
typedef enum {
    DfcEv2DirectionCommand,
    DfcEv2DirectionResponse,
} DfcEv2Direction;

void dfc_ev2_rotate_left(
    const uint8_t input[DFC_EV2_RANDOM_LENGTH],
    uint8_t output[DFC_EV2_RANDOM_LENGTH]);

// SesAuthENCKey and SesAuthMACKey from the static key and both challenges.
bool dfc_ev2_derive_session_keys(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t random_b[DFC_EV2_RANDOM_LENGTH],
    uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    uint8_t mac_key[DFC_AES_KEY_LENGTH]);

// An AES CMAC truncated to the eight odd-indexed octets EV2 puts on the wire.
bool dfc_ev2_wire_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* input,
    size_t input_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]);

// The MAC over lead || counter || TI || first || second, where `lead` is the
// instruction for a command and the status for a response. The body is taken
// in two parts so a clear header and an encrypted field need not be joined.
bool dfc_ev2_mac(
    const uint8_t mac_key[DFC_AES_KEY_LENGTH],
    uint8_t lead,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* first,
    size_t first_len,
    const uint8_t* second,
    size_t second_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]);

// The IV for one encrypted field: E(SesAuthENCKey, label || TI || counter || 0).
void dfc_ev2_derive_iv(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    uint8_t iv[DFC_AES_KEY_LENGTH]);

// Pads with 0x80 then zeroes, always adding at least one octet, and encrypts.
bool dfc_ev2_encrypt_data(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* encrypted,
    size_t encrypted_capacity,
    size_t* encrypted_len);

// Decrypts and strips the padding. With `allow_unpadded`, a field that carries
// no marker is returned whole, which CommitReaderID needs.
bool dfc_ev2_decrypt_data(
    const uint8_t encryption_key[DFC_AES_KEY_LENGTH],
    DfcEv2Direction direction,
    uint16_t counter,
    const uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH],
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* clear,
    size_t* clear_len,
    bool allow_unpadded);

// Comparison whose duration does not depend on where two MACs differ.
bool dfc_ev2_equal(const uint8_t* a, const uint8_t* b, size_t len);

#endif // DFC_ENABLE_EV2_SECURE_MESSAGING
