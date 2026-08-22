#include "bv_crypto.h"

#include <furi.h>
#include <furi_hal_crypto.h>
#include <furi_hal_random.h>
#include <string.h>

#define DEK_SIZE 32 // AES-256 data key
#define WRAP_IV_SIZE 16 // CBC IV for wrapping the DEK
#define GCM_IV_SIZE 12
#define GCM_TAG_SIZE 16

#define TAG "BioVaultCrypto"

#define KEK_SLOT FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT // slot 11, device-unique

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

// Some AEAD implementations dereference the AAD pointer even for length 0;
// pass a valid non-null pointer to be safe.
static const uint8_t EMPTY_AAD[1] = {0};

bool bv_crypto_enclave_selftest(void) {
    if(!furi_hal_crypto_enclave_ensure_key(KEK_SLOT)) {
        FURI_LOG_E(TAG, "enclave ensure_key(slot %u) failed", KEK_SLOT);
        return false;
    }

    // Fixed IV is fine here: this is a self-consistency round-trip, not storage.
    uint8_t iv[16];
    memset(iv, 0x24, sizeof(iv));
    const uint8_t pt[16] = "BioVault KEK t.."; // exactly 16 bytes
    uint8_t ct[16] = {0};
    uint8_t pt2[16] = {0};
    bool ok = false;

    if(furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) {
        bool enc = furi_hal_crypto_encrypt(pt, ct, sizeof(ct));
        furi_hal_crypto_enclave_unload_key(KEK_SLOT);
        if(enc && furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) {
            bool dec = furi_hal_crypto_decrypt(ct, pt2, sizeof(pt2));
            furi_hal_crypto_enclave_unload_key(KEK_SLOT);
            ok = dec && (memcmp(pt, pt2, sizeof(pt)) == 0);
        }
    }

    FURI_LOG_I(
        TAG,
        "enclave KEK CBC round-trip: %s (ct=%02X%02X%02X%02X..)",
        ok ? "PASS" : "FAIL",
        ct[0],
        ct[1],
        ct[2],
        ct[3]);
    return ok;
}

bool bv_crypto_gcm_kat(void) {
    uint8_t ct[16] = {0};
    uint8_t tag[16] = {0};
    FuriHalCryptoGCMState enc_st = furi_hal_crypto_gcm_encrypt_and_tag(
        KAT_KEY, KAT_IV, EMPTY_AAD, 0, KAT_PT, ct, sizeof(KAT_PT), tag);
    bool ct_ok = memcmp(ct, KAT_CT, sizeof(ct)) == 0;
    bool tag_ok = memcmp(tag, KAT_TAG, sizeof(tag)) == 0;
    bool enc_ok = (enc_st == FuriHalCryptoGCMStateOk) && ct_ok && tag_ok;
    FURI_LOG_I(
        TAG, "GCM KAT encrypt: state=%d ct_ok=%d tag_ok=%d", enc_st, ct_ok, tag_ok);

    uint8_t pt2[16] = {0};
    FuriHalCryptoGCMState dec_st = furi_hal_crypto_gcm_decrypt_and_verify(
        KAT_KEY, KAT_IV, EMPTY_AAD, 0, KAT_CT, pt2, sizeof(KAT_CT), KAT_TAG);
    bool pt_ok = memcmp(pt2, KAT_PT, sizeof(pt2)) == 0;
    bool dec_ok = (dec_st == FuriHalCryptoGCMStateOk) && pt_ok;
    FURI_LOG_I(TAG, "GCM KAT decrypt: state=%d pt_ok=%d", dec_st, pt_ok);

    // Negative check: a corrupted tag must be rejected (AuthFailure), never Ok.
    uint8_t bad_tag[16];
    memcpy(bad_tag, KAT_TAG, sizeof(bad_tag));
    bad_tag[0] ^= 0x01;
    FuriHalCryptoGCMState bad_st = furi_hal_crypto_gcm_decrypt_and_verify(
        KAT_KEY, KAT_IV, EMPTY_AAD, 0, KAT_CT, pt2, sizeof(KAT_CT), bad_tag);
    bool reject_ok = (bad_st == FuriHalCryptoGCMStateAuthFailure);
    FURI_LOG_I(TAG, "GCM KAT bad-tag reject: state=%d reject_ok=%d", bad_st, reject_ok);

    return enc_ok && dec_ok && reject_ok;
}

// Wrap a DEK under the slot-11 KEK (AES-CBC). wrapped must be DEK_SIZE bytes.
static bool bv_kek_wrap(const uint8_t iv[WRAP_IV_SIZE], const uint8_t* dek, uint8_t* wrapped) {
    if(!furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) return false;
    bool ok = furi_hal_crypto_encrypt(dek, wrapped, DEK_SIZE);
    furi_hal_crypto_enclave_unload_key(KEK_SLOT);
    return ok;
}

// Unwrap a DEK previously wrapped by bv_kek_wrap with the same iv.
static bool bv_kek_unwrap(const uint8_t iv[WRAP_IV_SIZE], const uint8_t* wrapped, uint8_t* dek) {
    if(!furi_hal_crypto_enclave_load_key(KEK_SLOT, iv)) return false;
    bool ok = furi_hal_crypto_decrypt(wrapped, dek, DEK_SIZE);
    furi_hal_crypto_enclave_unload_key(KEK_SLOT);
    return ok;
}

bool bv_crypto_dek_selftest(void) {
    if(!furi_hal_crypto_enclave_ensure_key(KEK_SLOT)) {
        FURI_LOG_E(TAG, "DEK test: enclave ensure_key failed");
        return false;
    }

    uint8_t dek[DEK_SIZE];
    uint8_t wrap_iv[WRAP_IV_SIZE];
    furi_hal_random_fill_buf(dek, sizeof(dek));
    furi_hal_random_fill_buf(wrap_iv, sizeof(wrap_iv));

    uint8_t wrapped[DEK_SIZE] = {0};
    uint8_t dek2[DEK_SIZE] = {0};
    bool wrap_ok = bv_kek_wrap(wrap_iv, dek, wrapped) && bv_kek_unwrap(wrap_iv, wrapped, dek2) &&
                   (memcmp(dek, dek2, DEK_SIZE) == 0);
    FURI_LOG_I(TAG, "DEK wrap/unwrap round-trip: %s", wrap_ok ? "PASS" : "FAIL");

    // Full chain: GCM-encrypt with the original DEK, decrypt with the UNWRAPPED
    // DEK. Proves the recovered key is byte-identical and drives GCM correctly.
    const uint8_t pt[24] = "biovault chain check.OK";
    uint8_t gcm_iv[GCM_IV_SIZE];
    furi_hal_random_fill_buf(gcm_iv, sizeof(gcm_iv));
    uint8_t ct[sizeof(pt)] = {0};
    uint8_t tag[GCM_TAG_SIZE] = {0};
    uint8_t pt2[sizeof(pt)] = {0};

    FuriHalCryptoGCMState enc_st = furi_hal_crypto_gcm_encrypt_and_tag(
        dek, gcm_iv, EMPTY_AAD, 0, pt, ct, sizeof(pt), tag);
    FuriHalCryptoGCMState dec_st = furi_hal_crypto_gcm_decrypt_and_verify(
        dek2, gcm_iv, EMPTY_AAD, 0, ct, pt2, sizeof(pt), tag);
    bool chain_ok = (enc_st == FuriHalCryptoGCMStateOk) &&
                    (dec_st == FuriHalCryptoGCMStateOk) && (memcmp(pt, pt2, sizeof(pt)) == 0);
    FURI_LOG_I(
        TAG, "KEK->DEK->GCM chain: %s (enc=%d dec=%d)", chain_ok ? "PASS" : "FAIL", enc_st, dec_st);

    // Wipe key material from the stack.
    memset(dek, 0, sizeof(dek));
    memset(dek2, 0, sizeof(dek2));

    return wrap_ok && chain_ok;
}
