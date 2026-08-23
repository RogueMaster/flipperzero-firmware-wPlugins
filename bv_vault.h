/*
 * BioVault vault codec + keystore.
 *   Keystore: [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32], DEK wrapped under enclave KEK.
 *   On-tag blob: [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext][tag:16] (AES-256-GCM).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bv_crypto.h"

// On-tag blob framing sizes.
#define BV_BLOB_HEADER 17 // magic(2)+ver(1)+nonce(12)+ct_len(2)
#define BV_BLOB_OVERHEAD (BV_BLOB_HEADER + BV_GCM_TAG_SIZE) // + tag

// Unlocked data key, RAM only. Zero when done (bv_vault_key_clear).
typedef struct {
    uint8_t dek[BV_DEK_SIZE];
} BvVaultKey;

// Load keystore and unwrap DEK into `out`; creates a fresh DEK on first use.
// False on enclave/storage failure.
bool bv_vault_key_open(BvVaultKey* out);

// Wipe a BvVaultKey from memory.
void bv_vault_key_clear(BvVaultKey* key);

// Derive NTAG PWD (4B) and PACK (2B) deterministically from DEK + UID.
void bv_vault_tag_password(
    const BvVaultKey* key,
    const uint8_t* uid,
    size_t uid_len,
    uint8_t pwd[4],
    uint8_t pack[2]);

// Encrypt `pt` into a vault blob; `blob` needs pt_len + BV_BLOB_OVERHEAD bytes.
bool bv_vault_encrypt(
    const BvVaultKey* key,
    const uint8_t* pt,
    size_t pt_len,
    uint8_t* blob,
    size_t* blob_len);

// Validate magic/version of a buffer starting with a vault blob and compute its
// framed length into `out_len`. False if invalid or it would exceed `avail`.
bool bv_vault_framed_len(const uint8_t* blob, size_t avail, size_t* out_len);

// Parse and decrypt a vault blob into `pt` (capacity pt_cap). False on bad
// framing or failed authentication.
bool bv_vault_decrypt(
    const BvVaultKey* key,
    const uint8_t* blob,
    size_t blob_len,
    uint8_t* pt,
    size_t pt_cap,
    size_t* pt_len);

// Round-trip a sample vault through encrypt -> parse -> decrypt.
bool bv_vault_selftest(void);
