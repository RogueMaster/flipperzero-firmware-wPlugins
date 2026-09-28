/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_RSA_ENCRYPT_H_
#define EXAMPLE_RSA_ENCRYPT_H_
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Encrypt with OAEP using SHA-256 for both hashes. The RNG supplies 32 bytes.
 * Provide TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits) scratch limbs and modulus-sized
 * ciphertext storage. Keep output, scratch and RNG state separate from inputs
 * and metadata. Use ciphertext only on TC_RSA_OK; used scratch is wiped. */
TC_RSA_result example_encrypt_rsa_oaep_sha256(const TC_RSA_public_key* key,
    TC_bytes label, TC_bytes plaintext, uint8_t* ciphertext, size_t ciphertext_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words);

#ifdef __cplusplus
}
#endif
#endif
