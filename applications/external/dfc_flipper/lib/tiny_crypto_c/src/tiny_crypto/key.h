/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_KEY_H_
#define TINY_CRYPTO_KEY_H_
#include <tiny_crypto/der.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_KEY_UNKNOWN, TC_KEY_RSA, TC_KEY_RSA_PSS, TC_KEY_EC,
  TC_KEY_DSA, TC_KEY_ED25519, TC_KEY_ED448, TC_KEY_X25519, TC_KEY_X448
} TC_key_type;

typedef enum { TC_SIGNATURE_ECDSA, TC_SIGNATURE_RSA_V15, TC_SIGNATURE_RSA_PSS } TC_signature_scheme;
/* Resolved signature parameters. mgf_hash and salt_length apply only to PSS;
 * salt_length is in bytes. hash identifies the supplied digest. */
typedef struct {
  TC_signature_scheme scheme;
  TC_hash_algorithm hash, mgf_hash;
  uint32_t salt_length;
} TC_signature_algorithm;

typedef struct {
  TC_DER_private_key container;
  TC_DER_rsa_private_key components;
  TC_key_type type;
} TC_KEY_rsa_private_key;

/* Read a DER PKCS #8 RSA key, retaining algorithm restrictions and borrowed
 * components. An embedded public key must match n/e. Check key mathematics
 * and attribute schemas separately. Keep out disjoint from encoded bytes;
 * failures preserve out. Requires TC_ENABLE_DER. */
TC_TLV_result TC_KEY_rsa_private_read(TC_bytes encoded,
                                     TC_KEY_rsa_private_key* out);
/* Check a successfully parsed, unchanged key's signature restrictions.
 * Keep the source buffer alive and stable. Apply application policy and
 * validate key mathematics before signing. This check performs no arithmetic. */
TC_TLV_result TC_KEY_rsa_private_signature_check(const TC_KEY_rsa_private_key* key,
                                                const TC_signature_algorithm* signature);
#ifdef __cplusplus
}
#endif
#endif
