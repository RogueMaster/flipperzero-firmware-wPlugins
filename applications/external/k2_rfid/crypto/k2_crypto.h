#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K2_CRYPTO_SECTOR_SIZE 48
#define K2_CRYPTO_KEY_SIZE    6
#define K2_CRYPTO_UID_SIZE    4

/**
 * @brief Derive Sector 1 Mifare Classic Key A / Key B from 4-byte tag UID.
 * 
 * Takes the 4-byte UID, replicates it 4 times to fill 16 bytes,
 * encrypts with AES-128-ECB using u_key ("q3bu^t1nqfZ(pf$1"),
 * and takes the first 6 bytes of the ciphertext as the key.
 *
 * @param uid 4-byte tag UID
 * @param key_out Buffer of at least 6 bytes to receive the derived key
 */
void k2_crypto_derive_key(const uint8_t uid[K2_CRYPTO_UID_SIZE], uint8_t key_out[K2_CRYPTO_KEY_SIZE]);

/**
 * @brief Encrypt 48 bytes of Sector 1 data (blocks 4, 5, 6).
 *
 * Uses AES-128-ECB with d_key ("H@CFkRnz@KAtBJp2").
 *
 * @param plain 48 bytes of plaintext ASCII spool data
 * @param cipher_out Buffer of at least 48 bytes to receive encrypted data
 */
void k2_crypto_encrypt_sector1(const uint8_t plain[K2_CRYPTO_SECTOR_SIZE], uint8_t cipher_out[K2_CRYPTO_SECTOR_SIZE]);

/**
 * @brief Decrypt 48 bytes of Sector 1 data (blocks 4, 5, 6).
 *
 * Uses AES-128-ECB with d_key ("H@CFkRnz@KAtBJp2").
 *
 * @param cipher 48 bytes of encrypted data from tag
 * @param plain_out Buffer of at least 48 bytes to receive decrypted ASCII spool data
 */
void k2_crypto_decrypt_sector1(const uint8_t cipher[K2_CRYPTO_SECTOR_SIZE], uint8_t plain_out[K2_CRYPTO_SECTOR_SIZE]);

/**
 * @brief Verify crypto operations against known test vectors.
 * @return true if self-test passes, false otherwise
 */
bool k2_crypto_self_test(void);

#ifdef __cplusplus
}
#endif
