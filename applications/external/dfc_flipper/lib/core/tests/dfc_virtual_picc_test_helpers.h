#pragma once

#include "munit/munit.h"
#include "dfc_virtual_picc.h"

void load_standard_credential(DfcCredential* credential);

void authenticate_legacy_key0(
    DfcVirtualPiccSession* session,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t expected_status);

// The same exchange for an arbitrary key number, so access-rights tests can open
// a session as the key a file names for one operation but not another.
void authenticate_legacy_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t expected_status);

size_t build_legacy_change_key_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* new_key,
    size_t new_key_len,
    uint8_t version,
    uint8_t* out);

size_t build_legacy_encrypted_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* out);

size_t build_legacy_change_other_key_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    uint8_t key_no,
    const uint8_t* old_key,
    const uint8_t* new_key,
    size_t key_len,
    uint8_t version,
    uint8_t* out);

// EV1 ISO/AES ChangeKey cryptogram: KeyNo || CBC(session, IV, clear).
// clear = (new XOR old if different key else new) [+ version if AES]
//       || CRC32(C4||KeyNo||that) [|| CRC32(new key) if different]
//       || zero pad to block size.
size_t build_ev1_change_key_payload(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    uint8_t* iv,
    uint8_t auth_key_no,
    uint8_t target_key_no,
    const uint8_t* current_key,
    const uint8_t* new_key,
    size_t key_len,
    bool aes_key,
    uint8_t new_version,
    uint8_t* out);

void authenticate_aes_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t* session_iv,
    uint8_t expected_status);

void authenticate_iso_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t* session_iv,
    uint8_t expected_status);
