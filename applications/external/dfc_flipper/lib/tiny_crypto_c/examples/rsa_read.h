/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_RSA_READ_H_
#define EXAMPLE_RSA_READ_H_
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Read a two-prime PKCS #1 key, validate its factors and CRT fields, then sign.
 * Keep DER and digest stable and separate from signature, scratch and RNG state.
 * Size scratch for the larger of validation and signing workspace requirements.
 * Signature length is the modulus size in bytes. Use it only on TC_RSA_OK. */
TC_RSA_result example_sign_rsa_der(TC_bytes der, TC_hash_algorithm hash, TC_bytes digest,
                                   TC_buffer signature, TC_random_source random,
                                   const TC_RSA_workspace* workspace);

/* PKCS #8 variant with the same buffer contract. Checks key restrictions
 * before validation and v1.5 signing. PSS-only keys return TC_RSA_INVALID. */
TC_RSA_result example_sign_rsa_pkcs8(TC_bytes der, TC_hash_algorithm hash, TC_bytes digest,
                                     TC_buffer signature, TC_random_source random,
                                     const TC_RSA_workspace* workspace);

/* Same PKCS #8 workflow using PSS, SHA-256/MGF1-SHA-256 and a 32-byte salt. */
TC_RSA_result example_sign_rsa_pkcs8_pss_sha256(TC_bytes der, TC_bytes digest, TC_buffer signature,
                                                TC_random_source random,
                                                const TC_RSA_workspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
