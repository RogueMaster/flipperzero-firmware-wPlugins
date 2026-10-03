/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Run-time hash selection by TC_hash_algorithm over the shared hash core.
 * The context is a TC_hash_context, which holds any enabled SHA context. */
#ifndef TC_HASH_DISPATCH_INTERNAL_H_
#define TC_HASH_DISPATCH_INTERNAL_H_
#include <tiny_crypto/common.h>
#include "hash_info_internal.h"
#include "hash_core_internal.h"
#include "internal.h"

#define TC_HASH_DISPATCH_ENABLED                                                                   \
  (TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || TC_ENABLE_SHA384 || TC_ENABLE_SHA512)

#if TC_HASH_DISPATCH_ENABLED
#include <tiny_crypto/hash.h>

static inline int tc_hash_available(TC_hash_algorithm algorithm)
{
  return tc_hash_core_lookup(algorithm) != NULL;
}

/* Keep the algorithm fixed between init and final. Scratch and input/output
 * storage are disjoint. Callers wipe scratch when a stream is abandoned. */
static inline TC_status tc_hash_init(TC_hash_algorithm algorithm, TC_hash_context* context)
{
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(algorithm);
  return info != NULL && context != NULL ? tc_hash_core_init(info, context) : TC_ERROR;
}

static inline TC_status tc_hash_update(TC_hash_algorithm algorithm, TC_hash_context* context,
                                       TC_bytes bytes)
{
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(algorithm);
  return info != NULL && context != NULL
             ? tc_hash_core_update(info, context, bytes.data, bytes.length)
             : TC_ERROR;
}

static inline TC_status tc_hash_update_parts(TC_hash_algorithm algorithm, TC_hash_context* context,
                                             const TC_bytes* parts, size_t count)
{
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(algorithm);
  return info != NULL && context != NULL ? tc_hash_core_update_parts(info, context, parts, count)
                                         : TC_ERROR;
}

/* The whole context is wiped on success and failure. */
static inline TC_status tc_hash_final(TC_hash_algorithm algorithm, TC_hash_context* context,
                                      uint8_t* digest)
{
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(algorithm);
  TC_status status =
      info != NULL && context != NULL ? tc_hash_core_final(info, context, digest) : TC_ERROR;
  if (context != NULL)
    TC_secure_zero(context, sizeof *context);
  return status;
}

/* Hash borrowed parts in order. Caller bounds count/lengths and validates ranges.
 * Context, digest and input storage are disjoint. Digest has the selected
 * hash's full output size. Digest is written only after all updates succeed. */
static inline TC_status tc_hash_digest_parts(TC_hash_algorithm algorithm, const TC_bytes* parts,
                                             size_t count, uint8_t* digest,
                                             TC_hash_context* context)
{
  TC_status status;
  if (!context || !digest || (count && !parts) || !tc_hash_available(algorithm))
    return TC_ERROR;
  status = tc_hash_init(algorithm, context);
  if (status == TC_OK)
    status = tc_hash_update_parts(algorithm, context, parts, count);
  if (status == TC_OK)
    return tc_hash_final(algorithm, context, digest);
  TC_secure_zero(context, sizeof *context);
  return status;
}

#else

/* Builds without SHA keep the dispatch interface so shared parsers compile.
 * Every operation reports TC_ERROR and the context is a placeholder. */
typedef union {
  uint8_t unused;
} TC_hash_context;

static inline int tc_hash_available(TC_hash_algorithm algorithm)
{
  (void)algorithm;
  return 0;
}

static inline TC_status tc_hash_init(TC_hash_algorithm algorithm, TC_hash_context* context)
{
  (void)algorithm;
  (void)context;
  return TC_ERROR;
}

static inline TC_status tc_hash_update(TC_hash_algorithm algorithm, TC_hash_context* context,
                                       TC_bytes bytes)
{
  (void)algorithm;
  (void)context;
  (void)bytes;
  return TC_ERROR;
}

static inline TC_status tc_hash_update_parts(TC_hash_algorithm algorithm, TC_hash_context* context,
                                             const TC_bytes* parts, size_t count)
{
  (void)algorithm;
  (void)context;
  (void)parts;
  (void)count;
  return TC_ERROR;
}

static inline TC_status tc_hash_final(TC_hash_algorithm algorithm, TC_hash_context* context,
                                      uint8_t* digest)
{
  (void)algorithm;
  (void)context;
  (void)digest;
  return TC_ERROR;
}

static inline TC_status tc_hash_digest_parts(TC_hash_algorithm algorithm, const TC_bytes* parts,
                                             size_t count, uint8_t* digest,
                                             TC_hash_context* context)
{
  (void)algorithm;
  (void)parts;
  (void)count;
  (void)digest;
  (void)context;
  return TC_ERROR;
}

#endif
#endif
