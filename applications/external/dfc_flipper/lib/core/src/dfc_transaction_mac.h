#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_common.h"

typedef enum {
    DfcTransactionMacOk = 0,
    DfcTransactionMacInvalid,
    DfcTransactionMacCapacity,
    DfcTransactionMacCryptoFailed,
} DfcTransactionMacStatus;

#define DFC_TRANSACTION_MAC_UID_MAX_LENGTH 7u

typedef struct {
    uint8_t* input;
    size_t input_length;
    size_t input_capacity;
    uint8_t key[DFC_AES_KEY_LENGTH];
    uint8_t uid[DFC_TRANSACTION_MAC_UID_MAX_LENGTH];
    size_t uid_length;
    uint32_t counter;
    uint32_t lifecycle_cookie;
    bool initialized;
} DfcTransactionMacVerifier;

// Transaction input is appended in the same order that commands completed on
// the card. Every integer below is serialized least-significant byte first.
// Each operation's header and data are independently zero-padded to a 16-byte
// boundary; a component already aligned to 16 bytes receives no extra block.
//
// Supported operations and headers are:
// - add_data: command || file number || 24-bit offset || 24-bit length. It
//   accepts ReadData and WriteData, plus native and ISO ReadRecords and
//   WriteRecord commands. The data component is the bytes read or written.
// - add_record_update: command || file number || 24-bit record number ||
//   24-bit offset || 24-bit length, followed by the update bytes. It accepts
//   native and ISO UpdateRecord commands.
// - add_value: command || file number || 32-bit value. It accepts GetValue,
//   Credit, Debit, and LimitedCredit.
// - add_clear_record: ClearRecordFile || file number.
// - add_reader_id: CommitReaderID || current 16-byte reader ID, followed by the
//   optional encrypted previous 16-byte reader ID.
//
// Read helpers receive the successful response data. Write and update helpers
// receive the command data. Callers omit operations that failed on the card.
// Verification authenticates this ordered input under the supplied key, UID,
// and returned transaction counter. Duplicate-receipt and counter-history
// policy remains the caller's responsibility.
//
// The caller owns `input` and chooses its capacity. Init accepts uninitialized
// verifier storage and overwrites it. Clear an initialized verifier before
// reinitializing when its accumulated input must also be wiped.
DfcTransactionMacStatus dfc_transaction_mac_verifier_init(
    DfcTransactionMacVerifier* verifier,
    uint8_t* input,
    size_t input_capacity,
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* uid,
    size_t uid_length,
    uint32_t counter);

void dfc_transaction_mac_verifier_clear(DfcTransactionMacVerifier* verifier);

DfcTransactionMacStatus dfc_transaction_mac_add_data(
    DfcTransactionMacVerifier* verifier,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);

DfcTransactionMacStatus dfc_transaction_mac_add_record_update(
    DfcTransactionMacVerifier* verifier,
    uint8_t command,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);

DfcTransactionMacStatus dfc_transaction_mac_add_value(
    DfcTransactionMacVerifier* verifier,
    uint8_t command,
    uint8_t file_number,
    uint32_t value);

DfcTransactionMacStatus
    dfc_transaction_mac_add_clear_record(DfcTransactionMacVerifier* verifier, uint8_t file_number);

DfcTransactionMacStatus dfc_transaction_mac_add_reader_id(
    DfcTransactionMacVerifier* verifier,
    const uint8_t current_reader_id[DFC_TRANSACTION_READER_ID_LENGTH],
    const uint8_t encrypted_previous_reader_id[DFC_TRANSACTION_READER_ID_LENGTH]);

DfcTransactionMacStatus dfc_transaction_mac_verify(
    const DfcTransactionMacVerifier* verifier,
    const uint8_t mac[DFC_WIRE_MAC_LENGTH],
    bool* verified);
