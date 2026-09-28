/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_HASH_DISPATCH_INTERNAL_H_
#define TC_HASH_DISPATCH_INTERNAL_H_
#include "hash_info_internal.h"
#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.h>
#endif

typedef union {
  uint8_t unused;
#if TC_ENABLE_SHA1
  struct TC_SHA1_ctx sha1;
#endif
#if TC_ENABLE_SHA224
  struct TC_SHA224_ctx sha224;
#endif
#if TC_ENABLE_SHA256
  struct TC_SHA256_ctx sha256;
#endif
#if TC_ENABLE_SHA384
  struct TC_SHA384_ctx sha384;
#endif
#if TC_ENABLE_SHA512
  struct TC_SHA512_ctx sha512;
#endif
} tc_hash_workspace;

static inline int tc_hash_available(TC_hash_algorithm algorithm)
{
  switch (algorithm) {
    case TC_HASH_SHA1: return TC_ENABLE_SHA1;
    case TC_HASH_SHA224: return TC_ENABLE_SHA224;
    case TC_HASH_SHA256: return TC_ENABLE_SHA256;
    case TC_HASH_SHA384: return TC_ENABLE_SHA384;
    case TC_HASH_SHA512: return TC_ENABLE_SHA512;
    default: return 0;
  }
}

typedef enum { TC_HASH_INIT, TC_HASH_UPDATE, TC_HASH_FINAL } tc_hash_operation;

/* Shared dispatch for contiguous, segmented and parser-fed messages. */
static inline TC_status tc_hash_process(TC_hash_algorithm algorithm,
    tc_hash_workspace* workspace, tc_hash_operation operation, TC_bytes bytes, uint8_t* digest)
{
  if (!workspace || (operation == TC_HASH_UPDATE && bytes.length && !bytes.data) ||
      (operation == TC_HASH_FINAL && !digest)) return TC_ERROR;
  /* Some profiles compile out every hash. */
  (void)bytes;
#define HASH_CASE(id, name, member) \
  case id: \
    switch (operation) { \
      case TC_HASH_INIT: return name##_init(&workspace->member); \
      case TC_HASH_UPDATE: return name##_update(&workspace->member,bytes.data,bytes.length); \
      case TC_HASH_FINAL: return name##_final(&workspace->member,digest); \
      default: return TC_ERROR; \
    }
  switch (algorithm) {
#if TC_ENABLE_SHA1
    HASH_CASE(TC_HASH_SHA1,TC_SHA1,sha1);
#endif
#if TC_ENABLE_SHA224
    HASH_CASE(TC_HASH_SHA224,TC_SHA224,sha224);
#endif
#if TC_ENABLE_SHA256
    HASH_CASE(TC_HASH_SHA256,TC_SHA256,sha256);
#endif
#if TC_ENABLE_SHA384
    HASH_CASE(TC_HASH_SHA384,TC_SHA384,sha384);
#endif
#if TC_ENABLE_SHA512
    HASH_CASE(TC_HASH_SHA512,TC_SHA512,sha512);
#endif
    default: break;
  }
#undef HASH_CASE
  return TC_ERROR;
}

/* Keep the algorithm fixed between init and final. Scratch and input/output
 * storage are disjoint. Callers wipe scratch when a stream is abandoned. */
static inline TC_status tc_hash_init(TC_hash_algorithm algorithm, tc_hash_workspace* workspace)
{
  const TC_bytes empty = {NULL,0};
  return tc_hash_process(algorithm,workspace,TC_HASH_INIT,empty,NULL);
}

static inline TC_status tc_hash_update(TC_hash_algorithm algorithm,
    tc_hash_workspace* workspace, TC_bytes bytes)
{
  return tc_hash_process(algorithm,workspace,TC_HASH_UPDATE,bytes,NULL);
}

static inline TC_status tc_hash_final(TC_hash_algorithm algorithm,
    tc_hash_workspace* workspace, uint8_t* digest)
{
  const TC_bytes empty = {NULL,0};
  TC_status status = tc_hash_process(algorithm,workspace,TC_HASH_FINAL,empty,digest);
  if (workspace) TC_secure_zero(workspace,sizeof *workspace);
  return status;
}

/* Hash borrowed parts in order. Caller bounds count/lengths and validates ranges.
 * Workspace, digest and input storage are disjoint. Digest has the selected
 * hash's full output size. No output is written before all updates succeed. */
static inline TC_status tc_hash_digest_parts(TC_hash_algorithm algorithm,
    const TC_bytes* parts, size_t count, uint8_t* digest, tc_hash_workspace* workspace)
{
  TC_status status;
  if (!workspace || !digest || (count && !parts) || !tc_hash_available(algorithm)) return TC_ERROR;
  for (size_t i = 0; i < count; ++i) if (parts[i].length && !parts[i].data) return TC_ERROR;
  status = tc_hash_init(algorithm,workspace);
  for (size_t i = 0; status == TC_OK && i < count; ++i)
    status = tc_hash_update(algorithm,workspace,parts[i]);
  if (status == TC_OK) return tc_hash_final(algorithm,workspace,digest);
  TC_secure_zero(workspace,sizeof *workspace);
  return status;
}
#endif
