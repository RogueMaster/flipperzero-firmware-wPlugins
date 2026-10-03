/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Public-key and signature algorithm identifiers shared by X.509, CMS, key
 * challenges and the signature providers.
 * Configuration: needs TC_ENABLE_DER for the DER-backed types.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_KEY_H_
#define TINY_CRYPTO_KEY_H_
#include <tiny_crypto/der.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_KEY_UNKNOWN,
  TC_KEY_RSA,
  TC_KEY_RSA_PSS,
  TC_KEY_EC,
  TC_KEY_DSA,
  TC_KEY_ED25519,
  TC_KEY_ED448,
  TC_KEY_X25519,
  TC_KEY_X448
} TC_key_type;

typedef enum { TC_SIGNATURE_ECDSA, TC_SIGNATURE_RSA_V15, TC_SIGNATURE_RSA_PSS } TC_signature_scheme;
/* Resolved signature parameters. mgf_hash and salt_length apply only to PSS.
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

#if TC_ENABLE_DER
/* Read a DER PKCS #8 RSA private key (RFC 5958 section 2) holding a PKCS #1
 * RSAPrivateKey (RFC 8017 appendix A.1.2). The algorithm is rsaEncryption
 * with NULL parameters (RFC 3279 section 2.3.1) or id-RSASSA-PSS with
 * optional RSASSA-PSS-params (RFC 4055 section 3.1), recorded in out->type.
 * An embedded version 1 public key must match n and e. All spans borrow
 * encoded, which holds secret key material. Keep it alive and unchanged while
 * out is used, and keep out disjoint from it. The reader charges no work.
 *
 * TC_TLV_ARGUMENT     NULL out, out overlapping encoded, or encoded with
 *                     NULL data and a length.
 * TC_TLV_INVALID      malformed DER, rsaEncryption parameters other than
 *                     NULL, malformed PSS parameters, or an embedded public
 *                     key with unused bits or other n or e.
 * TC_TLV_UNSUPPORTED  another key algorithm, a PKCS #8 version above 1, a
 *                     multi-prime key or a PSS mask other than MGF1.
 *
 * out changes only on TC_TLV_OK. Validate key mathematics with
 * TC_RSA_validate_private_key and attribute schemas separately. */
TC_TLV_result TC_KEY_rsa_private_read(TC_bytes encoded, TC_KEY_rsa_private_key* out);

/* Check that signature may be produced with key, a successfully read and
 * unchanged key. An rsaEncryption key allows PKCS #1 v1.5 and PSS. An
 * id-RSASSA-PSS key allows only PSS. Its parameters, when present, fix the
 * hash and MGF hash and set a minimum salt length (RFC 4055 section 3.3).
 * The source buffer of key stays alive and unchanged. Charges no work.
 *
 * TC_TLV_OK           the key permits the signature parameters.
 * TC_TLV_ARGUMENT     NULL key or signature.
 * TC_TLV_INVALID      a scheme the key type forbids, or PSS parameters that
 *                     break the key's restriction.
 * TC_TLV_UNSUPPORTED  an unknown scheme, or key restrictions with an
 *                     unrecognised hash OID.
 *
 * Apply application policy and validate key mathematics before signing. */
TC_TLV_result TC_KEY_rsa_private_signature_check(const TC_KEY_rsa_private_key* key,
                                                 const TC_signature_algorithm* signature);
#endif
#ifdef __cplusplus
}
#endif
#endif
