/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_COMMON_H_
#define TINY_CRYPTO_COMMON_H_

#include <tiny_crypto/config.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cryptographic APIs share a three-state result. Authentication failures are
 * separate from malformed arguments so protocols can reject packets without
 * treating normal hostile input as an internal error. */
typedef int TC_status;
/* Fill every requested byte from a cryptographically secure random source.
 * Return TC_OK only when the entire request was filled. */
typedef TC_status (*TC_random_fn)(void* user, uint8_t* output, size_t length);
/* Borrowed immutable bytes. Keep the backing storage alive and unchanged while
 * a reader or parsed view uses it. NULL data is valid only for an empty span. */
typedef struct {
  const uint8_t* data;
  size_t length;
} TC_bytes;

/* Caller-owned writable storage. capacity counts bytes. */
typedef struct {
  uint8_t* data;
  size_t capacity;
} TC_buffer;

typedef struct {
  TC_random_fn fill;
  void* context;
} TC_random_source;

/* Operations consume this shared budget, including failed attempts. */
typedef struct {
  uint32_t remaining;
} TC_work_budget;

/* Setup and resource-management results. */
typedef enum {
  TC_RESULT_OK, TC_RESULT_ARGUMENT, TC_RESULT_LIMIT,
  TC_RESULT_UNSUPPORTED, TC_RESULT_ERROR
} TC_result;

/* Credential validation combines signatures, trust policy and status evidence. */
typedef enum {
  TC_CREDENTIAL_VALID, TC_CREDENTIAL_INVALID, TC_CREDENTIAL_REVOKED,
  TC_CREDENTIAL_UNSUPPORTED, TC_CREDENTIAL_LIMIT, TC_CREDENTIAL_ERROR,
  /* A required trust or evidence source is unavailable. */
  TC_CREDENTIAL_UNAVAILABLE
} TC_credential_status;

/* Identifiers do not imply that the corresponding hash is enabled. */
typedef enum {
  TC_HASH_UNKNOWN, TC_HASH_SHA1, TC_HASH_SHA224, TC_HASH_SHA256,
  TC_HASH_SHA384, TC_HASH_SHA512
} TC_hash_algorithm;

typedef enum {
  TC_EC_UNKNOWN, TC_EC_P256, TC_EC_P384, TC_EC_P521, TC_EC_P224,
  TC_EC_SECP256K1, TC_EC_BRAINPOOL_P256, TC_EC_BRAINPOOL_P384, TC_EC_BRAINPOOL_P512,
  TC_EC_P192
} TC_EC_curve;

#define TC_ERROR    (-1)
#define TC_OK       0
#define TC_MISMATCH 1

#if (TC_ZEROIZE != 0) && (TC_ZEROIZE != 1)
#error "TC_ZEROIZE must be 0 or 1"
#endif

#if (TC_STRICT != 0) && (TC_STRICT != 1)
#error "TC_STRICT must be 0 or 1"
#endif

/* Best-effort secret wipe. memory must be valid for length bytes; NULL is
 * accepted only when length is zero. The compiler barrier prevents common
 * dead-store removal, but cannot clear copies already held in CPU registers. */
void TC_secure_zero(void* memory, size_t length);

/* Compare all public-length bytes without returning at the first mismatch. */
TC_status TC_ct_equal(const uint8_t* a, const uint8_t* b, size_t length);

#ifdef __cplusplus
}
#endif

#endif
