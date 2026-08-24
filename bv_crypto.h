/*
 * BioVault crypto - enclave key management + authenticated encryption.
 * Device-unique enclave KEK (AES-CBC) wraps a random 256-bit DEK; the DEK
 * (RAM-only during unlock) drives AES-256-GCM on the vault.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BV_DEK_SIZE     32 // AES-256 data key
#define BV_WRAP_IV_SIZE 16 // AES-CBC IV used to wrap the DEK under the KEK
#define BV_GCM_IV_SIZE  12 // AES-GCM nonce
#define BV_GCM_TAG_SIZE 16 // AES-GCM auth tag

// --- Key hierarchy primitives ---

// True if the secure enclave can be reached at all. The key store lives on the
// radio core (core2/FUS) and every enclave call is an SHCI round-trip to it, so
// a dead core2 means no device key. Checked before every enclave op: in fw
// 1.4.3 furi_hal_crypto_enclave_load_key()/store_key() take the crypto mutex
// and return early WITHOUT releasing it when core2 is down, deadlocking every
// later crypto call on the device.
bool bv_crypto_enclave_available(void);

// Ensure the slot-11 device-unique KEK exists. The firmware generates it on
// first use, so this succeeds on a factory-fresh Flipper; it fails only when
// the enclave is unreachable or its key store is corrupt. Idempotent.
bool bv_crypto_kek_ensure(void);

// Wrap a BV_DEK_SIZE DEK under the slot-11 KEK (AES-CBC). `wrapped` is BV_DEK_SIZE.
bool bv_crypto_kek_wrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* dek, uint8_t* wrapped);

// Unwrap a DEK wrapped by bv_crypto_kek_wrap with the same iv.
bool bv_crypto_kek_unwrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* wrapped, uint8_t* dek);

// Key stretching through the enclave: iterate buf = KEK-CBC(buf) `iters` times
// in-place (32 bytes). Each brute-force guess must repeat this on THIS device;
// no external hardware can accelerate it. Deterministic for a given iv/iters.
bool bv_crypto_kek_stretch(const uint8_t iv[BV_WRAP_IV_SIZE], uint8_t buf[32], uint32_t iters);

// AES-256-GCM seal: encrypt `pt`(len) into `ct`(len) and produce a BV_GCM_TAG_SIZE tag.
bool bv_crypto_gcm_seal(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* pt,
    size_t len,
    uint8_t* ct,
    uint8_t tag[BV_GCM_TAG_SIZE]);

// AES-256-GCM open: verify tag and decrypt `ct`(len) into `pt`(len). False if
// the tag does not authenticate; then `pt` must be discarded.
bool bv_crypto_gcm_open(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* ct,
    size_t len,
    const uint8_t tag[BV_GCM_TAG_SIZE],
    uint8_t* pt);

// --- Self-tests ---

// KEK AES-CBC encrypt->decrypt round-trip.
bool bv_crypto_enclave_selftest(void);

// AES-256-GCM known-answer test + decrypt/verify + tamper-rejection.
bool bv_crypto_gcm_kat(void);

// Full KEK->DEK->GCM chain round-trip.
bool bv_crypto_dek_selftest(void);
