#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <furi.h>
#include <furi_hal.h>
#include <lib/toolbox/bit_buffer.h>
#include <lib/toolbox/hex.h>

#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "cmac.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TWO_KEY_3DES_CBC_MODE   2
#define THREE_KEY_3DES_CBC_MODE 4
#define SHA1                    6
#define SHA256                  7
#define AES_128_CBC             9

#define SEOS_WORKER_MAX_BUFFER_SIZE 128
#define SEOS_WORKER_CMAC_SIZE       8

#define SEOS_APP_EXTENSION          ".seos"
/* The browser also lists raw dumps. Only for filtering: the macro above is
 * what builds and matches paths. */
#define SEOS_APP_BROWSER_EXTENSIONS ".seos|.bin"
#define SEOS_FILE_NAME_MAX_LENGTH   32

extern char* seos_file_header;
extern uint32_t seos_file_version;

typedef enum {
    BLE_PERIPHERAL,
    BLE_CENTRAL,
} BleMode;

typedef enum {
    FLOW_TEST,
    FLOW_READER,
    FLOW_CRED,
    FLOW_READER_SCANNER,
    FLOW_CRED_SCANNER,
    FLOW_INSPECT,
} FlowMode;

typedef enum {
    SELECT_AID,
    SELECT_ADF,
    GENERAL_AUTHENTICATION_1,
    GENERAL_AUTHENTICATION_2,
    REQUEST_SIO,
} SeosPhase;

typedef struct {
    uint8_t rndICC[8];
    uint8_t UID[8];
    uint8_t cNonce[16];
    uint8_t rNonce[16];
    uint8_t priv_key[16];
    uint8_t auth_key[16];
    uint8_t key_no;
    uint8_t cipher;
    uint8_t hash;
} AuthParameters;

/* Fills a session nonce with random bytes. Freshness of these is what stops a
 * session being replayed, so they must never be constant. */
void seos_worker_random_nonce(uint8_t* nonce, size_t len);

void seos_log_bitbuffer(char* TAG, char* prefix, BitBuffer* buffer);
void seos_log_buffer(char* TAG, char* prefix, uint8_t* buffer, size_t buffer_len);

void seos_worker_diversify_key(
    uint8_t master_key_value[16],
    uint8_t* diversifier,
    size_t diversifier_len,
    uint8_t* adf_oid,
    size_t adf_oid_len,
    uint8_t algo_id1,
    uint8_t algo_id2,
    uint8_t reference_qualifier,
    bool is_encryption,
    uint8_t* div_key);

/* Block size of a cipher, in bytes. Zero if it is not one we know. */
size_t seos_cipher_block_size(uint8_t cipher);

/* Encrypt, decrypt and authenticate under whichever cipher a session agreed.
 *
 * Callers had been writing the same three-armed ladder at every use; these
 * carry it once, and return false for a cipher they do not know rather than
 * quietly leaving the output as it was. */
bool seos_cipher_encrypt(
    uint8_t cipher,
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted);

bool seos_cipher_decrypt(
    uint8_t cipher,
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear);

bool seos_cipher_cmac(
    uint8_t cipher,
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* cmac);

/* The cipher wrappers return false on a bad key or a length that is not a
 * whole number of blocks. The output buffer is untouched in that case. */
bool seos_worker_aes_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear);
bool seos_worker_des_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear);

bool seos_worker_aes_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted);
bool seos_worker_des_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted);

#ifdef __cplusplus
}
#endif
