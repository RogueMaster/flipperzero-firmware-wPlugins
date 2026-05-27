/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZF_AAGUID_LEN 16
#define ZF_APP_ID "zerofido"
#define ZF_APP_DATA_ROOT "/ext/apps_data"
#define ZF_APP_DATA_DIR ZF_APP_DATA_ROOT "/" ZF_APP_ID

#ifndef ZF_RELEASE_DIAGNOSTICS
#define ZF_RELEASE_DIAGNOSTICS 0
#endif

#ifndef ZF_USB_DIAGNOSTICS
#define ZF_USB_DIAGNOSTICS 0
#endif

#ifndef ZF_AUTO_ACCEPT_REQUESTS
#define ZF_AUTO_ACCEPT_REQUESTS 0
#endif

#ifndef ZF_DEV_SCREENSHOT
#define ZF_DEV_SCREENSHOT 0
#endif

#ifndef ZF_DEV_FIDO2_1
#define ZF_DEV_FIDO2_1 0
#endif

#ifndef ZF_PACKED_ATTESTATION
#define ZF_PACKED_ATTESTATION 0
#endif

#ifndef ZF_HAS_SETTINGS_UI
#define ZF_HAS_SETTINGS_UI 1
#endif

#define ZF_CTAPHID_PACKET_SIZE 64
#define ZF_MAX_MSG_SIZE 1024
#define ZF_TRANSPORT_ARENA_SIZE (ZF_MAX_MSG_SIZE + 2U)
#ifdef ZF_HOST_TEST
#define ZF_COMMAND_SCRATCH_SIZE 6144U
#else
#define ZF_COMMAND_SCRATCH_SIZE 2880U
#endif
#define ZF_UI_SCRATCH_SIZE 2048U
#define ZF_APPROVAL_TIMEOUT_MS 30000
#define ZF_ASSERTION_QUEUE_TIMEOUT_MS 30000
#define ZF_KEEPALIVE_INTERVAL_MS 100
#define ZF_KEEPALIVE_UPNEEDED 0x02
#define ZF_ASSEMBLY_TIMEOUT_MS 3000

#define ZF_CREDENTIAL_ID_LEN 32
#define ZF_DESCRIPTOR_ID_DIGEST_LEN 32
#define ZF_MAX_DESCRIPTOR_ID_LEN 1023
#define ZF_PRIVATE_KEY_LEN 32
#define ZF_PUBLIC_KEY_LEN 32
#define ZF_ATTESTATION_CERT_MAX_SIZE 768
#define ZF_WRAP_IV_LEN 16
#define ZF_CLIENT_DATA_HASH_LEN 32
#define ZF_HMAC_SECRET_LEN 32
#define ZF_HMAC_SECRET_SALT_MAX_LEN 64
#define ZF_HMAC_SECRET_ENC_MAX_LEN (ZF_HMAC_SECRET_SALT_MAX_LEN + ZF_PIN_PROTOCOL2_IV_LEN)
#define ZF_HMAC_SECRET_AUTH_MAX_LEN 32
#define ZF_HMAC_SECRET_OUTPUT_MAX_LEN (64 + ZF_PIN_PROTOCOL2_IV_LEN)
#define ZF_MAX_CREDENTIALS 32
#define ZF_STORE_FORMAT_VERSION 1U
#define ZF_MAX_RP_ID_LEN 256
#define ZF_MAX_USER_ID_LEN 64
#define ZF_MAX_USER_NAME_LEN 65
#define ZF_MAX_DISPLAY_NAME_LEN 65
#define ZF_INDEX_DISPLAY_NAME_LEN 24
#define ZF_INDEX_RP_ID_LEN 32
#define ZF_MAX_ALLOW_LIST 32
#define ZF_PIN_AUTH_LEN 16
#define ZF_PIN_AUTH_MAX_LEN 32
#define ZF_PIN_PROTOCOL_V1 1U
#define ZF_PIN_PROTOCOL_V2 2U
#define ZF_PIN_HASH_LEN 16
#define ZF_PIN_TOKEN_LEN 32
#define ZF_PIN_PROTOCOL2_IV_LEN 16
#define ZF_PIN_ENCRYPTED_HASH_MAX_LEN (ZF_PIN_HASH_LEN + ZF_PIN_PROTOCOL2_IV_LEN)
#define ZF_PIN_ENCRYPTED_TOKEN_MAX_LEN (ZF_PIN_TOKEN_LEN + ZF_PIN_PROTOCOL2_IV_LEN)
#define ZF_PIN_NEW_PIN_BLOCK_MAX_LEN 64
#define ZF_PIN_ENCRYPTED_NEW_PIN_MAX_LEN (ZF_PIN_NEW_PIN_BLOCK_MAX_LEN + ZF_PIN_PROTOCOL2_IV_LEN)
#define ZF_PIN_RETRIES_MAX 8
#define ZF_MIN_PIN_LENGTH 4
#define ZF_PIN_TOKEN_TIMEOUT_MS 30000
#define ZF_COUNTER_RESERVATION_WINDOW 32U

#define ZF_PIN_PERMISSION_MC 0x01U
#define ZF_PIN_PERMISSION_GA 0x02U
/* Defined by CTAP; recognized only to reject unsupported feature permissions. */
#define ZF_PIN_PERMISSION_CM 0x04U
#define ZF_PIN_PERMISSION_BE 0x08U
#define ZF_PIN_PERMISSION_LBW 0x10U
#define ZF_PIN_PERMISSION_ACFG 0x20U

#define ZF_FIRMWARE_VERSION 700U

#define ZF_CRED_PROTECT_UV_OPTIONAL 0x01U
#define ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID 0x02U
#define ZF_CRED_PROTECT_UV_REQUIRED 0x03U

typedef uint32_t ZfTransportSessionId;

typedef enum {
    ZfAttestationModePacked = 0,
    ZfAttestationModeNone = 1,
} ZfAttestationMode;

typedef union {
    void *align_ptr;
    uint64_t align_u64;
    uint8_t bytes[ZF_COMMAND_SCRATCH_SIZE];
} ZfCommandScratchArena;

typedef union {
    void *align_ptr;
    uint64_t align_u64;
    uint8_t bytes[ZF_UI_SCRATCH_SIZE];
} ZfUiScratchArena;

/* CTAP command/status constants are kept local so protocol code avoids SDK coupling. */
enum {
    ZfCtapeCmdMakeCredential = 0x01,
    ZfCtapeCmdGetAssertion = 0x02,
    ZfCtapeCmdGetInfo = 0x04,
    ZfCtapeCmdClientPin = 0x06,
    ZfCtapeCmdReset = 0x07,
    ZfCtapeCmdGetNextAssertion = 0x08,
    ZfCtapeCmdSelection = 0x0B,
};

enum {
    ZF_CTAP_SUCCESS = 0x00,
    ZF_CTAP_ERR_INVALID_COMMAND = 0x01,
    ZF_CTAP_ERR_INVALID_PARAMETER = 0x02,
    ZF_CTAP_ERR_INVALID_LENGTH = 0x03,
    ZF_CTAP_ERR_INVALID_CHANNEL = 0x0B,
    ZF_CTAP_ERR_CBOR_UNEXPECTED_TYPE = 0x11,
    ZF_CTAP_ERR_INVALID_CBOR = 0x12,
    ZF_CTAP_ERR_MISSING_PARAMETER = 0x14,
    ZF_CTAP_ERR_CREDENTIAL_EXCLUDED = 0x19,
    ZF_CTAP_ERR_UNSUPPORTED_ALGORITHM = 0x26,
    ZF_CTAP_ERR_OPERATION_DENIED = 0x27,
    ZF_CTAP_ERR_KEY_STORE_FULL = 0x28,
    ZF_CTAP_ERR_UNSUPPORTED_OPTION = 0x2B,
    ZF_CTAP_ERR_INVALID_OPTION = 0x2C,
    ZF_CTAP_ERR_KEEPALIVE_CANCEL = 0x2D,
    ZF_CTAP_ERR_NO_CREDENTIALS = 0x2E,
    ZF_CTAP_ERR_USER_ACTION_TIMEOUT = 0x2F,
    ZF_CTAP_ERR_NOT_ALLOWED = 0x30,
    ZF_CTAP_ERR_PIN_INVALID = 0x31,
    ZF_CTAP_ERR_PIN_BLOCKED = 0x32,
    ZF_CTAP_ERR_PIN_AUTH_INVALID = 0x33,
    ZF_CTAP_ERR_PIN_AUTH_BLOCKED = 0x34,
    ZF_CTAP_ERR_PIN_NOT_SET = 0x35,
    ZF_CTAP_ERR_PIN_REQUIRED = 0x36,
    ZF_CTAP_ERR_PIN_POLICY_VIOLATION = 0x37,
    ZF_CTAP_ERR_PIN_TOKEN_EXPIRED = 0x38,
    ZF_CTAP_ERR_INVALID_SUBCOMMAND = 0x3E,
    ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION = 0x40,
    ZF_CTAP_ERR_OTHER = 0x7F,
};

/*
 * Full credential records are the durable signing objects. Index entries are a
 * privacy-preserving, compact projection used for lookup and UI lists; production
 * builds hash RP IDs and keep only short display labels in RAM.
 */
typedef struct {
    bool in_use;
    bool resident_key;
    char file_name[80];
    uint32_t storage_version;
    uint8_t credential_id[ZF_CREDENTIAL_ID_LEN];
    size_t credential_id_len;
    char rp_id[ZF_MAX_RP_ID_LEN];
    uint8_t user_id[ZF_MAX_USER_ID_LEN];
    size_t user_id_len;
    char user_name[ZF_MAX_USER_NAME_LEN];
    char user_display_name[ZF_MAX_DISPLAY_NAME_LEN];
    uint8_t public_x[ZF_PUBLIC_KEY_LEN];
    uint8_t public_y[ZF_PUBLIC_KEY_LEN];
    uint8_t private_wrapped[ZF_PRIVATE_KEY_LEN];
    uint8_t private_iv[ZF_WRAP_IV_LEN];
    uint32_t sign_count;
    uint32_t created_at;
    uint8_t cred_protect;
    bool hmac_secret;
    uint8_t hmac_secret_without_uv[ZF_HMAC_SECRET_LEN];
    uint8_t hmac_secret_with_uv[ZF_HMAC_SECRET_LEN];
} ZfCredentialRecord;

typedef struct {
    bool in_use;
    bool resident_key;
#ifdef ZF_HOST_TEST
    char file_name[80];
#endif
    uint8_t credential_id[ZF_CREDENTIAL_ID_LEN];
#ifdef ZF_HOST_TEST
    size_t credential_id_len;
    char rp_id[ZF_MAX_RP_ID_LEN];
    uint8_t user_id[ZF_MAX_USER_ID_LEN];
    size_t user_id_len;
    char user_name[ZF_MAX_USER_NAME_LEN];
    char user_display_name[ZF_MAX_DISPLAY_NAME_LEN];
#else
    uint8_t credential_id_len;
    uint8_t rp_id_hash[32];
#endif
    uint32_t sign_count;
    uint32_t counter_high_water;
    uint32_t created_at;
    uint8_t cred_protect;
} ZfCredentialIndexEntry;

typedef struct {
    ZfCredentialIndexEntry *records;
    size_t count;
    size_t capacity;
} ZfCredentialStore;

/* Assertion request data is shared by getAssertion and getNextAssertion replay. */
typedef struct {
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN];
    bool has_client_data_hash;
    char rp_id[ZF_MAX_RP_ID_LEN];
    bool has_hmac_secret;
    bool hmac_secret_has_pin_protocol;
    uint64_t hmac_secret_pin_protocol;
    uint8_t hmac_secret_platform_x[ZF_PUBLIC_KEY_LEN];
    uint8_t hmac_secret_platform_y[ZF_PUBLIC_KEY_LEN];
    uint8_t hmac_secret_salt_enc[ZF_HMAC_SECRET_ENC_MAX_LEN];
    size_t hmac_secret_salt_enc_len;
    uint8_t hmac_secret_salt_auth[ZF_HMAC_SECRET_AUTH_MAX_LEN];
    size_t hmac_secret_salt_auth_len;
} ZfAssertionRequestData;

typedef struct {
    bool active;
    ZfTransportSessionId session_id;
    bool uv_verified;
    bool user_present;
    uint16_t record_indices[ZF_MAX_CREDENTIALS];
    uint8_t count;
    uint8_t index;
    uint32_t expires_at;
    ZfAssertionRequestData request;
} ZfAssertionQueue;

/* Descriptor lists reference caller-owned scratch storage for parsed ID hashes. */
typedef struct {
    uint8_t credential_id_digest[ZF_DESCRIPTOR_ID_DIGEST_LEN];
} ZfCredentialDescriptor;

typedef struct {
    ZfCredentialDescriptor *entries;
    size_t count;
    size_t capacity;
} ZfCredentialDescriptorList;

/* Parsed makeCredential request with presence flags for CTAP optional fields. */
typedef struct {
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN];
    bool has_client_data_hash;
    char rp_id[ZF_MAX_RP_ID_LEN];
    char user_name[ZF_MAX_USER_NAME_LEN];
    char user_display_name[ZF_MAX_DISPLAY_NAME_LEN];
    uint8_t user_id[ZF_MAX_USER_ID_LEN];
    size_t user_id_len;
    ZfCredentialDescriptorList exclude_list;
    bool up;
    bool uv;
    bool rk;
    bool has_up;
    bool has_uv;
    bool has_rk;
    bool has_user_id;
    bool has_pubkey_cred_params;
    bool es256_supported;
    bool has_cred_protect;
    uint8_t cred_protect;
    uint8_t pin_auth[ZF_PIN_AUTH_MAX_LEN];
    size_t pin_auth_len;
    uint64_t pin_protocol;
    bool has_pin_auth;
    bool has_pin_protocol;
    bool hmac_secret_requested;
    bool has_attestation_format_preference;
    ZfAttestationMode preferred_attestation_mode;
} ZfMakeCredentialRequest;

/* Parsed getAssertion request plus allow-list/options/PIN metadata. */
typedef struct {
    ZfAssertionRequestData assertion;
    ZfCredentialDescriptorList allow_list;
    bool up;
    bool has_up;
    bool uv;
    bool has_uv;
    bool rk;
    bool has_rk;
    uint8_t pin_auth[ZF_PIN_AUTH_MAX_LEN];
    size_t pin_auth_len;
    uint64_t pin_protocol;
    bool has_pin_auth;
    bool has_pin_protocol;
} ZfGetAssertionRequest;
