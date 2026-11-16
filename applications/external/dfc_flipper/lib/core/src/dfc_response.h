#pragma once

// Allocation-free decoders for successful DESFire response payloads.
//
// These functions consume clear response data after status handling and secure
// messaging. They never consume the trailing native or ISO status bytes.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_common.h"

#if DFC_ENABLE_READER

#define DFC_RESPONSE_VERSION_LENGTH       28
#define DFC_RESPONSE_VERSION_UID_LENGTH   7
#define DFC_RESPONSE_VERSION_BATCH_LENGTH 5
#define DFC_RESPONSE_AID_LENGTH           3

typedef enum {
    DfcResponseDecodeOk = 0,
    DfcResponseDecodeMalformed,
    DfcResponseDecodeUnsupported,
    DfcResponseDecodeBufferTooSmall,
} DfcResponseDecodeStatus;

typedef enum {
    DfcResponseAccessKey = 0,
    DfcResponseAccessFree,
    DfcResponseAccessDenied,
} DfcResponseAccessKind;

typedef struct {
    DfcResponseAccessKind kind;
    // Meaningful only when kind is DfcResponseAccessKey.
    uint8_t key_number;
} DfcResponseAccess;

typedef struct {
    DfcResponseAccess read;
    DfcResponseAccess write;
    DfcResponseAccess read_write;
    DfcResponseAccess change;
} DfcResponseAccessRights;

typedef struct {
    uint8_t vendor_id;
    uint8_t type;
    uint8_t subtype;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t storage_size;
    uint8_t protocol;
} DfcResponseVersionPart;

typedef struct {
    DfcResponseVersionPart hardware;
    DfcResponseVersionPart software;
    uint8_t uid[DFC_RESPONSE_VERSION_UID_LENGTH];
    uint8_t batch_number[DFC_RESPONSE_VERSION_BATCH_LENGTH];
    uint8_t production_week;
    uint8_t production_year;
} DfcResponseVersion;

typedef struct {
    uint8_t settings_1;
    uint8_t settings_2;
    uint8_t key_type;
    uint8_t key_count;
    bool has_key_set_settings;
    uint8_t active_key_set_version;
    uint8_t key_set_count;
    uint8_t max_key_size;
    uint8_t key_set_settings;
} DfcResponseKeySettings;

typedef struct {
    uint8_t type;
    uint8_t communication_mode;
    // Always false on successful decode. SDM-enabled layouts currently return
    // DfcResponseDecodeUnsupported rather than omitting their option fields.
    bool sdm_enabled;
    uint16_t access_rights_raw;
    DfcResponseAccessRights access;
    union {
        struct {
            uint32_t size;
        } data;
        struct {
            int32_t lower_limit;
            int32_t upper_limit;
            int32_t limited_credit_value;
            bool limited_credit_enabled;
        } value;
        struct {
            uint32_t record_size;
            uint32_t max_records;
            uint32_t current_records;
        } records;
    } settings;
} DfcResponseFileSettings;

const char* dfc_response_decode_status_name(DfcResponseDecodeStatus status);

DfcResponseDecodeStatus dfc_response_decode_version(
    const uint8_t* response,
    size_t response_len,
    DfcResponseVersion* version);

// AIDs are returned in human-readable order. The decoder reverses the
// least-significant-octet-first wire representation used by DESFire.
DfcResponseDecodeStatus dfc_response_decode_application_ids(
    const uint8_t* response,
    size_t response_len,
    uint8_t (*aids)[DFC_RESPONSE_AID_LENGTH],
    size_t aid_capacity,
    size_t* aid_count);

DfcResponseDecodeStatus dfc_response_decode_file_ids(
    const uint8_t* response,
    size_t response_len,
    uint8_t* file_ids,
    size_t file_id_capacity,
    size_t* file_id_count);

DfcResponseDecodeStatus dfc_response_decode_file_settings(
    const uint8_t* response,
    size_t response_len,
    DfcResponseFileSettings* settings);

DfcResponseDecodeStatus dfc_response_decode_key_settings(
    const uint8_t* response,
    size_t response_len,
    DfcResponseKeySettings* settings);

DfcResponseDecodeStatus dfc_response_decode_key_version(
    const uint8_t* response,
    size_t response_len,
    uint8_t* key_version);

#endif
