/*
 * BioVault vault codec + keystore.
 *
 * Keystore (Flipper-local, /ext/apps_data/biovault/keystore.bin):
 *   [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32]
 * The DEK is stored wrapped under the slot-11 enclave KEK. Kept on the Flipper,
 * not the tag, so a future PIN change never requires a tag write. Auto-created
 * on first open. Losing this file (or the enclave) means the vault is
 * unrecoverable, by design (enclave-only, no escrow).
 *
 * On-tag vault blob (what actually gets written to Sector 1):
 *   [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext:ct_len][tag:16]
 * Length-prefixed and authenticated (AES-256-GCM), so there is no fragile carve
 * on runs of null bytes like the original Proxmark/awk pipeline.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bv_crypto.h"

// Fixed sizes of the on-tag blob framing.
#define BV_BLOB_HEADER 17 // magic(2)+ver(1)+nonce(12)+ct_len(2)
#define BV_BLOB_OVERHEAD (BV_BLOB_HEADER + BV_GCM_TAG_SIZE) // + tag

// The unlocked data key, held in RAM only. Zero it when done (bv_vault_key_clear).
typedef struct {
    uint8_t dek[BV_DEK_SIZE];
} BvVaultKey;

// Load the keystore and unwrap the DEK into `out`. Creates a fresh random DEK
// (and keystore file) on first use. Returns false on enclave/storage failure.
bool bv_vault_key_open(BvVaultKey* out);

// Wipe a BvVaultKey from memory.
void bv_vault_key_clear(BvVaultKey* key);

// Derive the NTAG PWD (4 bytes) and PACK (2 bytes) for an implant deterministically
// from the DEK and the tag's UID, so tag access is bound to this Flipper's
// enclave/keystore AND diversified per tag (NXP's recommendation). Same (key, uid)
// in -> same password out; nothing extra is persisted.
void bv_vault_tag_password(
    const BvVaultKey* key,
    const uint8_t* uid,
    size_t uid_len,
    uint8_t pwd[4],
    uint8_t pack[2]);

// Encrypt `pt`(pt_len) into a self-contained vault blob. `blob` must have room
// for pt_len + BV_BLOB_OVERHEAD bytes. Writes the blob length to `blob_len`.
bool bv_vault_encrypt(
    const BvVaultKey* key,
    const uint8_t* pt,
    size_t pt_len,
    uint8_t* blob,
    size_t* blob_len);

// Given a buffer that begins with a vault blob (e.g. a raw Sector 1 read, which
// is zero-padded past the blob), validate the magic/version and compute the full
// framed length (header + ciphertext + tag) into `out_len`. False if the header
// is not a valid vault blob or it would exceed `avail`.
bool bv_vault_framed_len(const uint8_t* blob, size_t avail, size_t* out_len);

// Parse and decrypt a vault blob into `pt` (capacity pt_cap). Writes plaintext
// length to `pt_len`. Returns false on bad framing or failed authentication.
bool bv_vault_decrypt(
    const BvVaultKey* key,
    const uint8_t* blob,
    size_t blob_len,
    uint8_t* pt,
    size_t pt_cap,
    size_t* pt_len);

// Self-test: open the keystore and round-trip a sample vault through
// encrypt -> blob -> parse -> decrypt. Runs at startup during bring-up.
bool bv_vault_selftest(void);
