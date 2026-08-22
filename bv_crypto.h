/*
 * BioVault crypto — enclave key management + authenticated encryption.
 *
 * Design (M2):
 *   - A device-unique KEK lives in the STM32WB secure enclave, slot 11
 *     (FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT). Generated on-device by the TRNG,
 *     never leaves the enclave, cannot be read back or cloned to another Flipper.
 *   - The KEK (AES-CBC via the enclave) wraps a random 256-bit DEK.
 *   - The DEK (in RAM only during an unlock) drives AES-256-GCM on the vault.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BV_DEK_SIZE 32 // AES-256 data key
#define BV_WRAP_IV_SIZE 16 // AES-CBC IV used to wrap the DEK under the KEK
#define BV_GCM_IV_SIZE 12 // AES-GCM nonce
#define BV_GCM_TAG_SIZE 16 // AES-GCM auth tag

// --- Key hierarchy primitives ---

// Ensure the slot-11 device-unique KEK exists (generated on first use). Idempotent.
bool bv_crypto_kek_ensure(void);

// Wrap a BV_DEK_SIZE DEK under the slot-11 KEK (AES-CBC). `wrapped` is BV_DEK_SIZE.
bool bv_crypto_kek_wrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* dek, uint8_t* wrapped);

// Unwrap a DEK wrapped by bv_crypto_kek_wrap with the same iv.
bool bv_crypto_kek_unwrap(const uint8_t iv[BV_WRAP_IV_SIZE], const uint8_t* wrapped, uint8_t* dek);

// AES-256-GCM seal: encrypt `pt`(len) into `ct`(len) and produce a BV_GCM_TAG_SIZE tag.
bool bv_crypto_gcm_seal(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* pt,
    size_t len,
    uint8_t* ct,
    uint8_t tag[BV_GCM_TAG_SIZE]);

// AES-256-GCM open: verify tag and decrypt `ct`(len) into `pt`(len). False if the
// tag does not authenticate (in which case `pt` must be discarded).
bool bv_crypto_gcm_open(
    const uint8_t dek[BV_DEK_SIZE],
    const uint8_t iv[BV_GCM_IV_SIZE],
    const uint8_t* ct,
    size_t len,
    const uint8_t tag[BV_GCM_TAG_SIZE],
    uint8_t* pt);

// --- Self-tests (run at startup during M2 bring-up) ---

// Bring up the slot-11 KEK and prove an AES-CBC encrypt->decrypt round-trip.
bool bv_crypto_enclave_selftest(void);

// AES-256-GCM known-answer test against a published vector + decrypt/verify +
// tamper-rejection.
bool bv_crypto_gcm_kat(void);

// Full KEK->DEK->GCM chain: random DEK, wrap/unwrap under the KEK, and prove the
// recovered DEK decrypts GCM data sealed under the original.
bool bv_crypto_dek_selftest(void);
