#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CK_CTAP2_CMD_MAKE_CREDENTIAL 0x01U
#define CK_CTAP2_CMD_GET_ASSERTION   0x02U
#define CK_CTAP2_CMD_GET_INFO        0x04U

#define CK_CTAP2_OK                        0x00U
#define CK_CTAP2_ERR_INVALID_COMMAND       0x01U
#define CK_CTAP2_ERR_INVALID_PARAMETER     0x02U
#define CK_CTAP2_ERR_INVALID_LENGTH        0x03U
#define CK_CTAP2_ERR_CREDENTIAL_EXCLUDED   0x19U
#define CK_CTAP2_ERR_INVALID_CREDENTIAL    0x22U
#define CK_CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26U
#define CK_CTAP2_ERR_OPERATION_DENIED      0x27U
#define CK_CTAP2_ERR_KEY_STORE_FULL        0x28U
#define CK_CTAP2_ERR_UNSUPPORTED_OPTION    0x2bU
#define CK_CTAP2_ERR_KEEPALIVE_CANCEL      0x2dU
#define CK_CTAP2_ERR_NO_CREDENTIALS        0x2eU
#define CK_CTAP2_ERR_INVALID_CBOR          0x12U
#define CK_CTAP2_ERR_MISSING_PARAMETER     0x14U
#define CK_CTAP2_ERR_OTHER                 0x7fU

#define CK_CTAP2_CREDENTIAL_RECORD_VERSION   1U
#define CK_CTAP2_CREDENTIAL_ID_SIZE          32U
#define CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET 1U
#define CK_CTAP2_CREDENTIAL_RECORD_SIZE      101U

typedef bool (*CkCtap2Random)(void* context, uint8_t* output, size_t length);
typedef bool (*CkCtap2UserPresent)(void* context);
typedef bool (*CkCtap2WasCancelled)(void* context);
typedef bool (*CkCtap2SaveCredential)(void* context, const uint8_t* record, size_t length);
typedef bool (*CkCtap2LoadCredential)(
    void* context,
    const uint8_t credential_id[CK_CTAP2_CREDENTIAL_ID_SIZE],
    uint8_t* record,
    size_t capacity,
    size_t* length);

typedef struct {
    void* context;
    CkCtap2Random random;
    CkCtap2UserPresent user_present;
    CkCtap2WasCancelled was_cancelled;
    CkCtap2SaveCredential save_credential;
    CkCtap2LoadCredential load_credential;
} CkCtap2Platform;

typedef struct {
    uint8_t credential_id[CK_CTAP2_CREDENTIAL_ID_SIZE];
    uint8_t rp_id_hash[32];
    uint8_t private_key[32];
    uint32_t sign_count;
} CkCtap2Credential;

typedef struct {
    CkCtap2Platform platform;
} CkCtap2;

void ck_ctap2_init(CkCtap2* authenticator, const CkCtap2Platform* platform);
bool ck_ctap2_handle(
    CkCtap2* authenticator,
    const uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_length);
bool ck_ctap2_credential_serialize(
    const CkCtap2Credential* credential,
    uint8_t* output,
    size_t capacity,
    size_t* output_length);
bool ck_ctap2_credential_deserialize(
    CkCtap2Credential* credential,
    const uint8_t* input,
    size_t input_length);
