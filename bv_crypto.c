#include "bv_crypto.h"

#include <furi.h>
#include <furi_hal_crypto.h>
#include <furi_hal_random.h>
#include <string.h>

#define TAG "BioVaultCrypto"

#define KEK_SLOT FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT // slot 11, device-unique

// Some AEAD implementations dereference the AAD pointer even for length 0;
// pass a valid non-null pointer to be safe.
static const uint8_t EMPTY_AAD[1] = {0};

// --- Key hierarchy primitives ---

bool bv_crypto_kek_ensure(void) {
    return furi_hal_crypto_enclave_ensure_key(KEK_SLOT);
}

bool bv_crypto_kek_wrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* dek, uint8_t* wrapped) {
    if(!furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) return false;
    bool ok = furi_hal_crypto_encrypt(dek, wrapped, BV_DEK_SIZE);
    furi_hal_crypto_enclave_unload_key(KEK_SLOT);
    return ok;
}

bool bv_crypto_kek_unwrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* wrapped, uint8_t* dek) {
    if(!furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) return false;
    bool ok = furi_hal_crypto_decrypt(wrapped, dek, BV_DEK_SIZE);
    furi_hal_crypto_enclave_unload_key(KEK_SLOT);
    return ok;
}

bool bv_crypto_gcm_seal(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* pt,
    size_t len,
    uint8_t* ct,
    uint8_t tag[BV_GCM_TAG_SIZE]) {
    return furi_hal_crypto_gcm_encrypt_and_tag(dek, iv, EMPTY_AAD, 0, pt, ct, len, tag) ==
           FuriHalCryptoGCMStateOk;
}

bool bv_crypto_gcm_open(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* ct,
    size_t len,
    const uint8_t tag[BV_GCM_TAG_SIZE],
    uint8_t* pt) {
    return furi_hal_crypto_gcm_decrypt_and_verify(dek, iv, EMPTY_AAD, 0, ct, pt, len, tag) ==
           FuriHalCryptoGCMStateOk;
}

// --- AES-256-GCM known-answer test vector ---
// GCM spec (McGrew & Viega) Appendix B, Test Case 14: AES-256, 96-bit IV,
// all-zero key/IV, single all-zero 16-byte plaintext block, no AAD.
static const uint8_t KAT_KEY[32] = {0};
static const uint8_t KAT_IV[12] = {0};
static const uint8_t KAT_PT[16] = {0};
static const uint8_t KAT_CT[16] = {
    0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e,
    0x07, 0x4e, 0xc5, 0xd3, 0xba, 0xf3, 0x9d, 0x18};
static const uint8_t KAT_TAG[16] = {
    0xd0, 0xd1, 0xc8, 0xa7, 0x99, 0x99, 0x6b, 0xf0,
    0x26, 0x5b, 0x98, 0xb5, 0xd4, 0x8a, 0xb9, 0x19};

// --- Self-tests ---

bool bv_crypto_enclave_selftest(void) {
    if(!bv_crypto_kek_ensure()) {
        FURI_LOG_E(TAG, "enclave ensure_key(slot %u) failed", KEK_SLOT);
        return false;
    }

    uint8_t iv[BV_WRAP_IV_SIZE];
    memset(iv, 0x24, sizeof(iv)); // fixed IV: self-consistency round-trip, not storage
    // kek_wrap/unwrap operate on a full DEK_SIZE block, so these must be DEK_SIZE.
    const uint8_t pt[BV_DEK_SIZE] = "BioVault KEK self-test block";
    uint8_t ct[BV_DEK_SIZE] = {0};
    uint8_t pt2[BV_DEK_SIZE] = {0};

    bool ok = bv_crypto_kek_wrap(iv, pt, ct) && bv_crypto_kek_unwrap(iv, ct, pt2) &&
              (memcmp(pt, pt2, sizeof(pt)) == 0);
    FURI_LOG_I(TAG, "enclave KEK CBC round-trip: %s", ok ? "PASS" : "FAIL");
    return ok;
}

bool bv_crypto_gcm_kat(void) {
    uint8_t ct[16] = {0};
    uint8_t tag[16] = {0};
    bool enc = bv_crypto_gcm_seal(KAT_KEY, KAT_IV, KAT_PT, sizeof(KAT_PT), ct, tag);
    bool ct_ok = memcmp(ct, KAT_CT, sizeof(ct)) == 0;
    bool tag_ok = memcmp(tag, KAT_TAG, sizeof(tag)) == 0;
    bool enc_ok = enc && ct_ok && tag_ok;
    FURI_LOG_I(TAG, "GCM KAT encrypt: seal=%d ct_ok=%d tag_ok=%d", enc, ct_ok, tag_ok);

    uint8_t pt2[16] = {0};
    bool dec_ok = bv_crypto_gcm_open(KAT_KEY, KAT_IV, KAT_CT, sizeof(KAT_CT), KAT_TAG, pt2) &&
                  (memcmp(pt2, KAT_PT, sizeof(pt2)) == 0);
    FURI_LOG_I(TAG, "GCM KAT decrypt: %s", dec_ok ? "PASS" : "FAIL");

    // Negative check: a corrupted tag must be rejected.
    uint8_t bad_tag[16];
    memcpy(bad_tag, KAT_TAG, sizeof(bad_tag));
    bad_tag[0] ^= 0x01;
    bool reject_ok = !bv_crypto_gcm_open(KAT_KEY, KAT_IV, KAT_CT, sizeof(KAT_CT), bad_tag, pt2);
    FURI_LOG_I(TAG, "GCM KAT bad-tag reject: %s", reject_ok ? "PASS" : "FAIL");

    return enc_ok && dec_ok && reject_ok;
}

bool bv_crypto_dek_selftest(void) {
    if(!bv_crypto_kek_ensure()) {
        FURI_LOG_E(TAG, "DEK test: enclave ensure_key failed");
        return false;
    }

    uint8_t dek[BV_DEK_SIZE];
    uint8_t wrap_iv[BV_WRAP_IV_SIZE];
    furi_hal_random_fill_buf(dek, sizeof(dek));
    furi_hal_random_fill_buf(wrap_iv, sizeof(wrap_iv));

    uint8_t wrapped[BV_DEK_SIZE] = {0};
    uint8_t dek2[BV_DEK_SIZE] = {0};
    bool wrap_ok = bv_crypto_kek_wrap(wrap_iv, dek, wrapped) &&
                   bv_crypto_kek_unwrap(wrap_iv, wrapped, dek2) &&
                   (memcmp(dek, dek2, BV_DEK_SIZE) == 0);
    FURI_LOG_I(TAG, "DEK wrap/unwrap round-trip: %s", wrap_ok ? "PASS" : "FAIL");

    // Full chain: seal with the original DEK, open with the UNWRAPPED DEK.
    const uint8_t pt[24] = "biovault chain check.OK";
    uint8_t gcm_iv[BV_GCM_IV_SIZE];
    furi_hal_random_fill_buf(gcm_iv, sizeof(gcm_iv));
    uint8_t ct[sizeof(pt)] = {0};
    uint8_t tag[BV_GCM_TAG_SIZE] = {0};
    uint8_t pt2[sizeof(pt)] = {0};

    bool chain_ok = bv_crypto_gcm_seal(dek, gcm_iv, pt, sizeof(pt), ct, tag) &&
                    bv_crypto_gcm_open(dek2, gcm_iv, ct, sizeof(pt), tag, pt2) &&
                    (memcmp(pt, pt2, sizeof(pt)) == 0);
    FURI_LOG_I(TAG, "KEK->DEK->GCM chain: %s", chain_ok ? "PASS" : "FAIL");

    memset(dek, 0, sizeof(dek));
    memset(dek2, 0, sizeof(dek2));
    return wrap_ok && chain_ok;
}
