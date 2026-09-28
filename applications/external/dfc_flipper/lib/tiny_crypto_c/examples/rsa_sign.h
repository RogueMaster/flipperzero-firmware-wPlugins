/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_RSA_SIGN_H_
#define EXAMPLE_RSA_SIGN_H_
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_RSA_PSS_SHA256_BYTES = 32 };

/* Key components have already passed validation and remain unchanged.
 * Keep signature, scratch and RNG state separate from the key and digest.
 * Use the signature only on TC_RSA_OK. */
TC_RSA_result example_sign_rsa_v15_digest(const TC_RSA_private_key* key,
    TC_hash_algorithm hash, TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words);

/* Same buffer contract. PSS with SHA-256, MGF1-SHA-256 and a 32-byte salt.
 * Enable SHA-256 for PSS hashing. */
TC_RSA_result example_sign_rsa_pss_sha256_digest(const TC_RSA_private_key* key,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words);

#ifdef __cplusplus
}
#endif
#endif
