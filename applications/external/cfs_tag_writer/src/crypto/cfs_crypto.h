#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <nfc/protocols/mf_classic/mf_classic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CFS_CRYPTO_BLOCK_SIZE     (16)
#define CFS_CRYPTO_PLAINTEXT_SIZE (48) // 3 AES blocks

// Standard MIFARE transport access bits for the CFS sector-1 trailer (4 bytes:
// access(3) + GPB). Shared by the tag writer and the .nfc dump builder.
extern const uint8_t cfs_access_bits[4];

/**
 * Derive the MIFARE Classic sector-1 Key A from a tag UID.
 *
 * The UID bytes are tiled to fill a 16-byte buffer, AES-128 encrypted with the
 * hardcoded CFS "u_key", and the first 6 bytes of the result are the key.
 */
void cfs_crypto_derive_key_a(const uint8_t* uid, size_t uid_len, MfClassicKey* out_key);

/**
 * Derive the MIFARE Classic sector-1 Key B from a tag UID.
 *
 * The CFS spec states Key B is "usually same as Key A or default"; we
 * derive it identically to Key A.
 */
void cfs_crypto_derive_key_b(const uint8_t* uid, size_t uid_len, MfClassicKey* out_key);

/** Decrypt the 48-byte CFS payload (blocks 4/5/6) into plaintext. */
void cfs_crypto_decrypt(
    const uint8_t ciphertext[CFS_CRYPTO_PLAINTEXT_SIZE],
    uint8_t plaintext[CFS_CRYPTO_PLAINTEXT_SIZE]);

/** Encrypt the 48-byte CFS plaintext payload into ciphertext for blocks 4/5/6. */
void cfs_crypto_encrypt(
    const uint8_t plaintext[CFS_CRYPTO_PLAINTEXT_SIZE],
    uint8_t ciphertext[CFS_CRYPTO_PLAINTEXT_SIZE]);

/**
 * Round-trip self-test against a known-good CFS example.
 *
 * Validates encrypt->decrypt self-consistency only; it does NOT validate
 * against a real captured ciphertext (none exists in the spec). Logs results
 * via FURI_LOG. Returns true if the round trip is byte-identical.
 */
bool cfs_crypto_self_test(void);

#ifdef __cplusplus
}
#endif
