/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Public-key proof-of-possession challenges: fresh random challenges and
 * verification of the signed response under a validated public key.
 * Standards: FIPS 186-5, RFC 8017 signature schemes.
 * Configuration: TC_ENABLE_KEY_CHALLENGE, with RSA from TC_ENABLE_RSA.
 * Limitations: card commands, slot policy and transport identifiers belong
 * to protocol code.
 * Contracts: docs/api.md, including its TC_work_budget units.
 * Guide: docs/credential-reader.md. */
#ifndef TINY_CRYPTO_KEY_CHALLENGE_H_
#define TINY_CRYPTO_KEY_CHALLENGE_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Results follow the TC_RSA_result and TC_EC_result order. INVALID is a bad
 * proof. LIMIT is exhausted work. ARGUMENT is a NULL pointer, overlapping
 * storage or verify without an active challenge. UNSUPPORTED is a key, scheme
 * or size outside the supported set. ERROR is a random-source or provider
 * failure. */
typedef TC_result TC_key_challenge_result;
#define TC_KEY_CHALLENGE_OK TC_RESULT_OK
#define TC_KEY_CHALLENGE_INVALID TC_RESULT_INVALID
#define TC_KEY_CHALLENGE_LIMIT TC_RESULT_LIMIT
#define TC_KEY_CHALLENGE_ARGUMENT TC_RESULT_ARGUMENT
#define TC_KEY_CHALLENGE_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_KEY_CHALLENGE_ERROR TC_RESULT_ERROR

typedef struct {
  TC_signature_algorithm signature;
} TC_key_challenge_options;

/* Storage bounds for one challenge. The input holds an RSA encoded message
 * of the modulus length, or an ECDSA digest. PSS salts are at most one
 * SHA-512 digest long. */
enum {
  TC_KEY_CHALLENGE_MAX_DIGEST_BYTES = 64,
  TC_KEY_CHALLENGE_MAX_SALT_BYTES = 64,
#if TC_ENABLE_RSA
  TC_KEY_CHALLENGE_MAX_INPUT_BYTES = TC_RSA_MAX_MODULUS_BYTES
#else
  TC_KEY_CHALLENGE_MAX_INPUT_BYTES = TC_KEY_CHALLENGE_MAX_DIGEST_BYTES
#endif
};

/* Caller-owned state for one proof-of-possession challenge. Keep key and
 * provider inputs alive and unchanged until verify or clear. The returned
 * challenge borrows this storage. Do not copy an active workspace. */
typedef struct {
  uint8_t digest[TC_KEY_CHALLENGE_MAX_DIGEST_BYTES];
  uint8_t challenge[TC_KEY_CHALLENGE_MAX_INPUT_BYTES];
  TC_signature_algorithm signature;
  size_t digest_length;
  uint32_t state;
} TC_key_challenge_workspace;

#if TC_ENABLE_KEY_CHALLENGE
/* Start a challenge: draw a fresh random digest from random and prepare the
 * input for the card's private-key operation. For RSA, *out is the encoded
 * message of the modulus length: EMSA-PKCS1-v1_5 (RFC 8017 section 9.2) or
 * EMSA-PSS with a random salt (RFC 8017 section 9.1.1). For ECDSA, *out is
 * the digest itself for signing under FIPS 186-5 section 6.4.1. *out borrows
 * workspace and stays valid until verify, clear or the next prepare.
 *
 * Supported keys are RSA moduli accepted by TC_RSA_modulus_supported, with
 * PKCS #1 v1.5 or PSS and a salt of at most TC_KEY_CHALLENGE_MAX_SALT_BYTES,
 * and ECDSA keys on a named curve identified by the X.509 key decoder. The
 * signature provider decides at verify whether it implements an identified
 * curve. The key, options and random context stay disjoint from workspace,
 * work and out.
 *
 * TC_KEY_CHALLENGE_ARGUMENT     NULL key, options, random.fill, workspace,
 *                               work or out, or overlap.
 * TC_KEY_CHALLENGE_UNSUPPORTED  unknown hash, MGF hash or salt set outside
 *                               PSS, another key type or scheme, an RSA size
 *                               outside the supported set, a larger salt or
 *                               an ECDSA key with curve TC_EC_UNKNOWN.
 * TC_KEY_CHALLENGE_LIMIT        work below the full cost.
 * TC_KEY_CHALLENGE_ERROR        a random request failed.
 *
 * ARGUMENT, UNSUPPORTED and LIMIT leave workspace, out and work unchanged
 * and make no RNG request. A later failure wipes workspace and consumes the
 * work spent. out changes only on TC_KEY_CHALLENGE_OK. Work: the digest
 * length, plus the salt length for PSS, plus TC_RSA_encode_v15_work or
 * TC_RSA_encode_pss_work for RSA. */
TC_key_challenge_result TC_key_challenge_prepare(const TC_X509_public_key* key,
                                                 const TC_key_challenge_options* options,
                                                 TC_random_source random,
                                                 TC_key_challenge_workspace* workspace,
                                                 TC_work_budget* work, TC_bytes* out);

/* Verify the card's response against the retained digest with
 * TC_X509_signature_verify_digest, then wipe workspace. key is the same
 * public key passed to prepare. signature is borrowed and uses the
 * provider's encoding: an RSA signature of the modulus length or a DER
 * ECDSA-Sig-Value. key, provider state and signature stay disjoint from
 * workspace.
 *
 * TC_KEY_CHALLENGE_ARGUMENT     NULL workspace or no active challenge, with
 *                               workspace unchanged. With an active
 *                               challenge: NULL key, provider or work, an
 *                               empty signature or storage overlapping
 *                               workspace, with workspace wiped and work
 *                               unchanged.
 * TC_KEY_CHALLENGE_INVALID      the proof fails verification.
 * TC_KEY_CHALLENGE_UNSUPPORTED  the provider lacks the key's curve, scheme
 *                               or size.
 * TC_KEY_CHALLENGE_LIMIT        work ran out.
 * TC_KEY_CHALLENGE_ERROR        the provider failed.
 *
 * Every result except the first ARGUMENT case wipes workspace, so each
 * challenge verifies at most once. Work: the charges of
 * TC_X509_signature_verify_digest, including the provider's own work. */
TC_key_challenge_result TC_key_challenge_verify(const TC_X509_public_key* key, TC_bytes signature,
                                                const TC_X509_signature_provider* provider,
                                                TC_key_challenge_workspace* workspace,
                                                TC_work_budget* work);

/* Abandon an active challenge and wipe the whole workspace. Accepts NULL.
 * Charges no work. */
void TC_key_challenge_clear(TC_key_challenge_workspace* workspace);
#endif

#ifdef __cplusplus
}
#endif
#endif
