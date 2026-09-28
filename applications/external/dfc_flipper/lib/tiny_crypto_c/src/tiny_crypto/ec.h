/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

/* Scalars and coordinates are fixed-width, big-endian (24, 32 or 48 bytes).
 * Public keys use SEC 1 uncompressed encoding, 04 || X || Y. Lengths must
 * match the selected curve exactly. Output and workspace must not overlap
 * each other or any input. On failure output is unchanged. Scratch is wiped
 * after use. Unsupported curves return TC_ERROR. */
TC_status TC_EC_public_key(TC_EC_curve curve, const uint8_t* scalar, size_t scalar_len,
    uint8_t* output, size_t output_len, TC_EC_workspace* workspace);
TC_status TC_EC_validate_public_key(TC_EC_curve curve, const uint8_t* public_key,
    size_t public_key_len, TC_EC_workspace* workspace);
/* Returns the shared point's X coordinate, including leading zero bytes.
 * Pass this value through the protocol's key derivation function before use. */
TC_status TC_ECDH(TC_EC_curve curve, const uint8_t* scalar, size_t scalar_len,
    const uint8_t* public_key, size_t public_key_len, uint8_t* output,
    size_t output_len, TC_EC_workspace* workspace);

/* Verify a precomputed digest with a SEC 1 uncompressed public key.
 * Signature encoding is fixed-width big-endian r || s, not DER. Digests
 * longer than the curve order are truncated to their leftmost bytes.
 * TC_MISMATCH means the signature or public key is invalid; TC_ERROR means
 * an invalid argument or unsupported curve. Workspace must be disjoint
 * from all inputs and is wiped after use. Both high and low s are accepted. */
TC_status TC_ECDSA_verify_digest(TC_EC_curve curve,
    const uint8_t* public_key, size_t public_key_len,
    const uint8_t* digest, size_t digest_len,
    const uint8_t* signature, size_t signature_len,
    TC_ECDSA_workspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
