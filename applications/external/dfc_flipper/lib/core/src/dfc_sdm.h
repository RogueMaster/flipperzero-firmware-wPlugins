#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DFC_SDM_VERIFY_AES_KEY_LENGTH 16
#define DFC_SDM_VERIFY_UID_MAX_LENGTH 10

typedef enum {
    DfcSdmVerifyOk,
    DfcSdmVerifyMalformed,
    DfcSdmVerifyUnsupported,
    DfcSdmVerifyAuthenticationFailed,
    DfcSdmVerifyBufferTooSmall,
    DfcSdmVerifyCryptoFailed,
} DfcSdmVerifyStatus;

typedef struct {
    bool present;
    size_t offset;
} DfcSdmVerifyField;

typedef struct {
    // Plain UID length is 7 or 10. Set to zero when encrypted PICC data carries it.
    size_t plain_uid_length;
    DfcSdmVerifyField plain_uid;
    DfcSdmVerifyField plain_read_counter;
    DfcSdmVerifyField encrypted_picc_data;
    size_t encrypted_picc_data_length;
    DfcSdmVerifyField encrypted_file_data;
    size_t encrypted_file_data_length;
    size_t mac_input_offset;
    size_t mac_offset;
} DfcSdmVerifyConfig;

typedef struct {
    bool has_uid;
    uint8_t uid[DFC_SDM_VERIFY_UID_MAX_LENGTH];
    size_t uid_length;
    bool has_read_counter;
    uint32_t read_counter;
    size_t decrypted_file_data_length;
} DfcSdmVerifyResult;

// Verifies one ASCII-hex SDM response. The MAC covers message bytes from
// mac_input_offset up to, but not including, mac_offset. Verified UID/counter
// fields are published only after the MAC succeeds.
// Encrypted PICC data is one AES block, except a 10-byte UID without a read
// counter, which uses two blocks. Counter-only encrypted PICC data is accepted.
//
// meta_read_key is required for encrypted PICC data. file_read_key is required
// for the MAC and encrypted file data. decrypted_file_data may be NULL to query
// the required capacity. Input and decrypted output must not overlap. On
// malformed or unauthenticated input, result is cleared and decrypted output is
// left untouched.
DfcSdmVerifyStatus dfc_sdm_verify_ascii(
    const uint8_t* message,
    size_t message_length,
    const DfcSdmVerifyConfig* config,
    const uint8_t meta_read_key[DFC_SDM_VERIFY_AES_KEY_LENGTH],
    const uint8_t file_read_key[DFC_SDM_VERIFY_AES_KEY_LENGTH],
    uint8_t* decrypted_file_data,
    size_t decrypted_file_data_capacity,
    size_t* required_decrypted_file_data_length,
    DfcSdmVerifyResult* result);
