/*
 * BioVault vault codec + keystore.
 *   Keystore v1: [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32], DEK wrapped under enclave KEK.
 *   Keystore v2 (PIN set): [magic 'BVK2':4][salt:16][sw_iters:4 LE][hw_iters:4 LE]
 *   [wrap_iv:16][wrapped:32] where wrapped = KEK-CBC(DEK XOR unlock_key) and
 *   unlock_key = bv_pin_derive(PIN, salt). No verifier by design: a wrong PIN
 *   yields a garbage DEK detectable only by the vault's GCM tag, so a stolen
 *   Flipper alone offers nothing to test PIN guesses against.
 *   On-tag blob: [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext][tag:16] (AES-256-GCM).
 *   ver 2 plaintext is heatshrink-compressed (toolbox compress framing) to
 *   stretch the tag's fixed capacity; ver 1 (raw plaintext) is still readable.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bv_crypto.h"

// On-tag blob framing sizes.
#define BV_BLOB_HEADER   17 // magic(2)+ver(1)+nonce(12)+ct_len(2)
#define BV_BLOB_OVERHEAD (BV_BLOB_HEADER + BV_GCM_TAG_SIZE) // + tag

// Unlocked data key, RAM only. Zero when done (bv_vault_key_clear).
typedef struct {
    uint8_t dek[BV_DEK_SIZE];
} BvVaultKey;

// Load keystore and unwrap DEK into `out`; creates a fresh DEK on first use.
// False on enclave/storage failure, or if a PIN is required but no unlock key
// has been set this session.
bool bv_vault_key_open(BvVaultKey* out);

// --- PIN (unlock key) management ---

// True if the keystore is v2 (PIN-wrapped) and needs an unlock key.
bool bv_vault_pin_required(void);

// Fetch v2 KDF parameters. False if the keystore is not v2.
bool bv_vault_pin_params(uint8_t salt[16], uint32_t* sw_iters, uint32_t* hw_iters);

// Cache the session unlock key (from bv_pin_derive) / wipe it.
void bv_vault_unlock_key_set(const uint8_t key[32]);
void bv_vault_unlock_key_clear(void);

// Rewrap the DEK under a new PIN (v1->v2 or new v2 params) / back to v1.
// Both unwrap with the CURRENT session state first - callers must ensure the
// current unlock key is proven (a GCM-verified load) before rewrapping, or a
// wrong-PIN session would orphan the on-tag vault.
bool bv_vault_pin_enable(const char* pin);
bool bv_vault_pin_disable(void);

// Wipe a BvVaultKey from memory.
void bv_vault_key_clear(BvVaultKey* key);

// Derive NTAG PWD (4B) and PACK (2B) deterministically from DEK + UID.
void bv_vault_tag_password(
    const BvVaultKey* key,
    const uint8_t* uid,
    size_t uid_len,
    uint8_t pwd[4],
    uint8_t pack[2]);

// Compress + encrypt `pt` into a vault blob; `blob` needs pt_len + 1 +
// BV_BLOB_OVERHEAD bytes (worst case: incompressible data stored raw).
bool bv_vault_encrypt(
    const BvVaultKey* key,
    const uint8_t* pt,
    size_t pt_len,
    uint8_t* blob,
    size_t* blob_len);

// Validate magic/version of a buffer starting with a vault blob and compute its
// framed length into `out_len`. False if invalid or it would exceed `avail`.
bool bv_vault_framed_len(const uint8_t* blob, size_t avail, size_t* out_len);

// Parse, decrypt, and (v2) decompress a vault blob into `pt` (capacity
// pt_cap). False on bad framing, failed authentication, or pt_cap overrun.
bool bv_vault_decrypt(
    const BvVaultKey* key,
    const uint8_t* blob,
    size_t blob_len,
    uint8_t* pt,
    size_t pt_cap,
    size_t* pt_len);

// Round-trip a sample vault through encrypt -> parse -> decrypt.
bool bv_vault_selftest(void);
