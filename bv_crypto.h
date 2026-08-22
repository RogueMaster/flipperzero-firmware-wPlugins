/*
 * BioVault crypto — enclave key management + authenticated encryption.
 *
 * Design (M2):
 *   - A device-unique KEK lives in the STM32WB secure enclave, slot 11
 *     (FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT). Generated on-device by the TRNG,
 *     never leaves the enclave, cannot be read back or cloned to another Flipper.
 *   - The KEK (AES-CBC via the enclave) wraps a random 256-bit DEK.
 *   - The DEK (in RAM only during an unlock) drives AES-256-GCM on the vault.
 *
 * This header currently exposes the M2 step-1 self-tests. Wrap/unwrap of the DEK
 * and the vault codec land in the next steps.
 */
#pragma once

#include <stdbool.h>

// Bring up the enclave slot-11 KEK (generate on first use) and prove it works
// with an AES-CBC encrypt->decrypt round-trip. Returns true on success.
bool bv_crypto_enclave_selftest(void);

// AES-256-GCM known-answer test against a published NIST/GCM-spec vector, plus a
// decrypt-and-verify round-trip. Proves furi_hal_crypto's GCM matches spec
// (byte order, tag handling) before we trust it with real data.
bool bv_crypto_gcm_kat(void);

// Full key-hierarchy round-trip: generate a random DEK, wrap it under the
// slot-11 enclave KEK (AES-CBC), unwrap it, and prove the recovered DEK
// decrypts data that was GCM-encrypted under the original. Exercises the whole
// KEK -> DEK -> AES-GCM chain we build the vault on. Returns true on success.
bool bv_crypto_dek_selftest(void);
