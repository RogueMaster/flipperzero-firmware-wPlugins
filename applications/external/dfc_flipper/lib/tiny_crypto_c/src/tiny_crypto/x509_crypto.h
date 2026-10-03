/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Native signature provider for the X.509 APIs: ECDSA and RSA PKCS #1 v1.5
 * and PSS verification over caller workspaces.
 * Standards: FIPS 186-5, RFC 8017, RFC 5758, RFC 4055.
 * Configuration: TC_ENABLE_X509 with TC_ENABLE_EC or TC_ENABLE_RSA and the
 * hashes the certificates use.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/x509-crypto.md. */
#ifndef TINY_CRYPTO_X509_CRYPTO_H_
#define TINY_CRYPTO_X509_CRYPTO_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/ec.h>
#include <tiny_crypto/rsa.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Default signature_work. Covers the supported curves and RSA sizes with
 * ordinary public exponents. */
#define TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK 32768u

/* Scratch and work reservation for the native provider. ec and rsa are
 * caller-owned. Either may be NULL when its algorithm is unused. Size rsa
 * with TC_RSA_VERIFY_WORKSPACE_WORDS for the largest accepted modulus.
 * signature_work is charged from the shared size_t budget for each crypto
 * attempt and bounds the EC or RSA operation's own work units. ECDSA needs
 * at least 64 units per curve bit. */
typedef struct {
  TC_ECDSA_workspace* ec;
  const TC_RSA_workspace* rsa;
  size_t signature_work;
} TC_X509_native_workspace;

#if TC_ENABLE_X509
/* Return a provider for TC_X509_signature_verify, the message and digest
 * verifiers and path validation. It verifies ECDSA with DER ECDSA-Sig-Value
 * signatures (RFC 5758 section 3.2, FIPS 186-5 section 6.4.2) and RSA PKCS #1
 * v1.5 and PSS (RFC 8017 sections 8.2.2 and 8.1.2, RFC 4055 section 3). The
 * returned provider borrows workspace, so keep the descriptor and its scratch
 * alive while the provider is used. Building the provider charges no work.
 * The descriptor is checked on each call.
 *
 * Each provider call returns:
 *   VALID        the signature verifies under the key.
 *   INVALID      a malformed signature, a scheme the key forbids, PSS
 *                parameters that break the key's restriction, or a failed
 *                verification.
 *   UNSUPPORTED  an algorithm, hash, curve or RSA size disabled in this
 *                build, a key on an unidentified curve, or an unknown OID.
 *   LIMIT        the size_t budget ran out, or signature_work is below the
 *                operation's own cost.
 *   ERROR        a NULL descriptor, key, algorithm or work, a missing
 *                workspace for the key's algorithm, scratch or metadata
 *                overlapping an input or the work counter, a digest of the
 *                wrong length, or a hash failure.
 *
 * Work: the provider charges the byte length of every message segment or
 * the digest, the DER algorithm OID and parameters, the key's algorithm,
 * key, modulus, exponent and curve_oid spans and the signature, then
 * signature_work. This is in addition to the charges of the calling X.509
 * function. The provider never increases *work.
 *
 * Calls sharing scratch must be serialized. Message verification hashes the
 * borrowed segments in order with one stack hash context and digest. Digest
 * verification uses the digest as supplied. PSS still needs its signature
 * and MGF hashes. ECDSA scratch is wiped after each operation, and RSA
 * scratch follows the RSA verification rules. Trust and application policy
 * are separate steps. */
TC_X509_signature_provider TC_X509_native_provider(const TC_X509_native_workspace* workspace);
#endif

#ifdef __cplusplus
}
#endif
#endif
