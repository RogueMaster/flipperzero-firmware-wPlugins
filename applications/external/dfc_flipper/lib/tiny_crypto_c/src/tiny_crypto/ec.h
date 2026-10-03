/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Elliptic-curve operations on P-192, P-256 and P-384: key generation,
 * public-key derivation and validation, ECDH and ECDSA.
 * Standards: FIPS 186-5, SP 800-186, SP 800-56A Rev. 3, SEC 1.
 * Configuration: TC_ENABLE_EC, TC_EC_ENABLE_P192/P256/P384 and TC_EC_SMALL.
 * Limitations: uncompressed SEC 1 points only. P-192 is off by default.
 * Contracts: docs/api.md, including its TC_work_budget units. Guide: docs/ec.md. */
#ifndef TINY_CRYPTO_EC_H_
#define TINY_CRYPTO_EC_H_
#include <tiny_crypto/common.h>

#if TC_EC_SMALL || defined(__AVR__)
typedef uint8_t TC_EC_word;
#define TC_EC_WORD_BITS 8
#else
typedef uint32_t TC_EC_word;
#define TC_EC_WORD_BITS 32
#endif
#if TC_EC_ENABLE_P384
#define TC_EC_MAX_BYTES 48
#else
#define TC_EC_MAX_BYTES 32
#endif
#define TC_EC_MAX_WORDS (TC_EC_MAX_BYTES / (TC_EC_WORD_BITS / 8))

/* Scratch storage for one operation. Members are private to the implementation.
 * Each concurrent operation needs its own workspace. */
typedef struct {
  TC_EC_word fields[30][TC_EC_MAX_WORDS];
  TC_EC_word product[2 * TC_EC_MAX_WORDS + 2];
  TC_EC_word reduced[TC_EC_MAX_WORDS];
} TC_EC_workspace;

typedef struct {
  TC_EC_workspace ec;
  TC_EC_word scalars[2][TC_EC_MAX_WORDS];
  TC_EC_word point[3][TC_EC_MAX_WORDS];
} TC_ECDSA_workspace;

#ifdef __cplusplus
extern "C" {
#endif

/* Results. Every EC function checks its arguments once, in this order, and
 * reports the first problem it finds:
 *
 *   TC_EC_ARGUMENT     NULL pointers, an empty digest and overlapping storage.
 *   TC_EC_UNSUPPORTED  a curve that is unknown or disabled in this build.
 *   TC_EC_INVALID      a private key, public key or signature whose length
 *                      does not match the curve.
 *   TC_EC_LIMIT        a caller output buffer shorter than required, then
 *                      too little work or too few random attempts.
 *
 * After these checks, TC_EC_INVALID also reports a private scalar outside
 * [1, n - 1], a point that is not on the curve and a signature that does not
 * verify. TC_EC_ERROR reports a random source failure or a signature that
 * failed its own verification. Output buffers larger than required are
 * accepted, and exactly the documented length is written. Outputs change only
 * on TC_EC_OK. UNSUPPORTED and LIMIT never report success. Argument errors
 * and limits found before arithmetic leave the workspace, the work budget and
 * the random source untouched. */
typedef TC_result TC_EC_result;
#define TC_EC_OK TC_RESULT_OK
#define TC_EC_INVALID TC_RESULT_INVALID
#define TC_EC_LIMIT TC_RESULT_LIMIT
#define TC_EC_ARGUMENT TC_RESULT_ARGUMENT
#define TC_EC_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_EC_ERROR TC_RESULT_ERROR

/* Randomized operations draw from random, making at most random_attempts
 * requests. Work is reduced by the units completed on success and failure. */
typedef TC_execution TC_EC_execution;

typedef enum {
  TC_EC_OPERATION_PUBLIC_KEY,
  TC_EC_OPERATION_VALIDATE,
  TC_EC_OPERATION_ECDH,
  TC_EC_OPERATION_VERIFY,
  TC_EC_OPERATION_SIGN,    /* one nonce attempt, including self-verification */
  TC_EC_OPERATION_GENERATE /* one scalar attempt */
} TC_EC_operation;

typedef struct {
  TC_hash_algorithm hash;
  size_t candidate_attempts;
} TC_ECDSA_sign_options;

#if TC_ENABLE_EC
/* Work units for one operation, or one attempt of a randomized operation, on
 * curve. A scalar multiplication or a modular inversion costs one unit per
 * curve bit; point validation and each random request cost one unit. Zero for
 * an unsupported curve or unknown operation. An operation checks its full cost
 * before it starts and returns LIMIT, with work unchanged, when the budget is
 * short. */
uint32_t TC_EC_operation_work(TC_EC_curve curve, TC_EC_operation operation);

/* Bytes in one coordinate or private scalar of curve: 24 for P-192, 32 for
 * P-256 and 48 for P-384. Zero for a curve that is unknown or disabled in this
 * build. A SEC 1 uncompressed public key is 1 + 2 * width bytes and a fixed
 * r || s signature is 2 * width bytes. */
size_t TC_EC_coordinate_bytes(TC_EC_curve curve);
#endif

/* Shared conventions for the functions below. Scalars and coordinates are
 * fixed-width, big-endian values of TC_EC_coordinate_bytes(curve) bytes, w
 * below. Public keys use SEC 1 section 2.3.3 uncompressed encoding, 04 || X ||
 * Y, of 1 + 2w bytes. Signatures are fixed-width r || s of 2w bytes. Input
 * lengths must match the curve exactly. Outputs, the workspace and the work
 * budget or execution descriptor are pairwise disjoint and disjoint from every
 * input. The workspace is wiped after arithmetic starts, on success and
 * failure. Each call first checks its cost from TC_EC_operation_work. After
 * that check, the charged work stays consumed whatever the result. */

#if TC_ENABLE_EC
/* Derive the SEC 1 public key Q = dG for private scalar d (SEC 1 section
 * 3.2.1). public_key.capacity is at least 1 + 2w, and exactly 1 + 2w bytes are
 * written, only on TC_EC_OK. Point multiplication runs in constant work.
 *
 * TC_EC_ARGUMENT     NULL workspace, work or span data, or overlap.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_INVALID      private_key.length differs from w, or, after the work
 *                    charge, d lies outside [1, n - 1].
 * TC_EC_LIMIT        short public_key, or work below
 *                    TC_EC_operation_work(curve, TC_EC_OPERATION_PUBLIC_KEY).
 *
 * Work: TC_EC_OPERATION_PUBLIC_KEY, 2 units per curve bit. */
TC_EC_result TC_EC_public_key(TC_EC_curve curve, TC_bytes private_key, TC_buffer public_key,
                              TC_EC_workspace* workspace, TC_work_budget* work);

/* Generate a private scalar uniform in [1, n - 1] and its SEC 1 public key
 * (SEC 1 section 3.2.1). Each attempt draws w bytes from execution->random
 * and discards an out-of-range draw. private_key needs w bytes and
 * public_key 1 + 2w. Both are written together, only on TC_EC_OK. The RNG
 * must be cryptographically secure, fill each request completely and keep
 * its context disjoint from the outputs and workspace. Stack copies of the
 * candidate are wiped on return.
 *
 * TC_EC_ARGUMENT     NULL workspace, execution, random.fill or buffer data,
 *                    or overlap.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_LIMIT        a short output, zero random_attempts, work below one
 *                    attempt's cost, or every allowed attempt rejected.
 * TC_EC_ERROR        a random request failed.
 *
 * Work: TC_EC_OPERATION_GENERATE per attempt, charged before its RNG
 * request. */
TC_EC_result TC_EC_generate_key_pair(TC_EC_curve curve, TC_buffer private_key, TC_buffer public_key,
                                     TC_EC_workspace* workspace, TC_EC_execution* execution);

/* Validate an uncompressed public key (SEC 1 section 3.2.2.1, SP 800-56A
 * Rev. 3 section 5.6.2.3.3). The coordinates must be below p and satisfy the
 * curve equation. The supported curves have cofactor 1, so this completes
 * full public-key validation.
 *
 * TC_EC_ARGUMENT     NULL workspace, work or key data, or overlap of the
 *                    workspace with the key or work.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_INVALID      length other than 1 + 2w, or, after the work charge, a
 *                    first byte other than 04 or a point off the curve.
 * TC_EC_LIMIT        work below TC_EC_operation_work(curve,
 *                    TC_EC_OPERATION_VALIDATE).
 *
 * Work: TC_EC_OPERATION_VALIDATE, 1 unit. */
TC_EC_result TC_EC_validate_public_key(TC_EC_curve curve, TC_bytes public_key,
                                       TC_EC_workspace* workspace, TC_work_budget* work);

/* ECC CDH primitive (SP 800-56A Rev. 3 section 5.7.1.2). Validates the peer
 * key as TC_EC_validate_public_key does, then writes the X coordinate of dQ
 * with leading zero bytes, w bytes, only on TC_EC_OK. shared_secret.capacity
 * is at least w. Pass the value through the protocol's key derivation
 * function before use.
 *
 * TC_EC_ARGUMENT     NULL workspace, work or span data, or overlap.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_INVALID      private_key.length differs from w, peer_public_key has
 *                    another length or a first byte other than 04, or,
 *                    after the work charge, d outside [1, n - 1], a peer point
 *                    off the curve or a shared point at infinity.
 * TC_EC_LIMIT        short shared_secret, or work below
 *                    TC_EC_operation_work(curve, TC_EC_OPERATION_ECDH).
 *
 * Work: TC_EC_OPERATION_ECDH, 2 units per curve bit plus 1. */
TC_EC_result TC_ECDH(TC_EC_curve curve, TC_bytes private_key, TC_bytes peer_public_key,
                     TC_buffer shared_secret, TC_EC_workspace* workspace, TC_work_budget* work);

/* Verify an ECDSA signature over a precomputed digest (FIPS 186-5 section
 * 6.4.2). Convert DER signatures to r || s first. A digest longer than w
 * bytes is truncated to its leftmost w bytes. Both high and low s are
 * accepted. Verification branches on public scalar bits. The public key is
 * validated as part of verification. Key, digest and signature may share
 * storage.
 *
 * TC_EC_ARGUMENT     NULL workspace, work or span data, an empty digest, or
 *                    workspace or work overlapping each other or an input.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_INVALID      public key or signature of the wrong length, or, after
 *                    the work charge, r or s outside [1, n - 1], an invalid
 *                    public key or a signature that fails verification.
 * TC_EC_LIMIT        work below TC_EC_operation_work(curve,
 *                    TC_EC_OPERATION_VERIFY).
 *
 * Work: TC_EC_OPERATION_VERIFY, 4 units per curve bit plus 1. */
TC_EC_result TC_ECDSA_verify_digest(TC_EC_curve curve, TC_bytes public_key, TC_bytes digest,
                                    TC_bytes signature, TC_ECDSA_workspace* workspace,
                                    TC_work_budget* work);

/* Sign a precomputed digest with private scalar d and an externally generated
 * nonce (FIPS 186-5 section 6.4.1).
 * public_key is the matching SEC 1 key. Each attempt draws a w-byte secret
 * nonce from execution->random and retries an out-of-range nonce, r = 0 or
 * s = 0 within execution->random_attempts. The RNG must be
 * cryptographically secure, fill each request completely and keep its
 * context disjoint from the other arguments. A digest longer than w bytes is
 * truncated to its leftmost w bytes. signature.capacity is at least 2w, and
 * exactly 2w bytes are written, only on TC_EC_OK. With TC_ECDSA_SIGN_VERIFY
 * (default 1) the signature is verified against public_key before release.
 *
 * TC_EC_ARGUMENT     NULL workspace, execution, random.fill or span data, an
 *                    empty digest, or overlap.
 * TC_EC_UNSUPPORTED  unknown or disabled curve.
 * TC_EC_INVALID      private or public key of the wrong length, or, after
 *                    the LIMIT preflight, d outside [1, n - 1].
 * TC_EC_LIMIT        short signature, zero random_attempts, work below one
 *                    attempt's cost, or every allowed attempt rejected.
 * TC_EC_ERROR        a random request failed, or self-verification failed
 *                    because of a fault or a mismatched public key.
 *
 * The LIMIT preflight for the first attempt runs before any workspace write.
 * Work: TC_EC_OPERATION_SIGN per attempt, 3 units per curve bit plus 1, and
 * 4 units per curve bit plus 1 more with TC_ECDSA_SIGN_VERIFY. */
TC_EC_result TC_ECDSA_sign_digest_external_random(TC_EC_curve curve, TC_bytes private_key,
                                                  TC_bytes public_key, TC_bytes digest,
                                                  TC_buffer signature,
                                                  TC_ECDSA_workspace* workspace,
                                                  TC_EC_execution* execution);

/* Sign with the deterministic nonce generation procedure from RFC 6979
 * section 3.2. options.hash identifies the hash that produced digest, whose
 * length must match that hash. candidate_attempts bounds the RFC 6979 retry
 * sequence for the negligible r = 0, s = 0, or out-of-range cases. This API
 * uses the enabled internal hash implementation and does not require HMAC to
 * be exposed as a public feature. private_key, public_key, digest, signature,
 * workspace and work must be disjoint as documented for the external-random
 * API. The same key, digest and options always produce the same signature.
 *
 * TC_EC_ARGUMENT     NULL options, workspace, work or span data, an empty
 *                    digest, a digest length unlike options.hash, or overlap.
 * TC_EC_UNSUPPORTED  unknown or disabled curve or hash.
 * TC_EC_INVALID      private or public key of the wrong length, or d outside
 *                    [1, n - 1].
 * TC_EC_LIMIT        short signature, zero candidate_attempts, insufficient
 *                    work, or every allowed candidate rejected.
 * TC_EC_ERROR        internal nonce generation or self-verification failed. */
TC_EC_result TC_ECDSA_sign_digest(TC_EC_curve curve, const TC_ECDSA_sign_options* options,
                                  TC_bytes private_key, TC_bytes public_key, TC_bytes digest,
                                  TC_buffer signature, TC_ECDSA_workspace* workspace,
                                  TC_work_budget* work);
#endif

#ifdef __cplusplus
}
#endif
#endif
