/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/sskdf.h>
#include "internal.h"
#include <string.h>
#if TC_ENABLE_SSKDF
#include "hash_dispatch_internal.h"

/* Caller-owned hash storage for one derivation. ctx is the typed context for
 * the algorithm, and digest holds one digest. */
typedef struct {
  void* ctx;
  size_t ctx_size;
  uint8_t* digest;
} sskdf_state;

/* Derives exactly out.capacity bytes by the SP 800-56C Rev. 2 section 4.1
 * process. The descriptor may name any enabled SHA. */
static TC_status derive(const tc_hash_algorithm_info* hash, const sskdf_state* state,
                        TC_bytes secret, const TC_bytes* info, size_t count, TC_buffer out)
{
  void* ctx = state->ctx;
  uint8_t* digest = state->digest;
  const uint8_t* z = secret.data;
  const size_t z_len = secret.length;
  uint8_t* output = out.data;
  const size_t output_len = out.capacity;
  uint8_t counter[4];
  size_t total, i, offset = 0, take;
  uint32_t round = 1;
  const size_t digest_length = tc_hash_core_digest_bytes(hash);
  TC_status status = TC_ERROR;
  if (!z || !z_len || !output || !output_len || (!info && count) ||
      count > SIZE_MAX / sizeof *info || z_len > SIZE_MAX - 4 ||
      !tc_internal_ranges_disjoint(z, z_len, output, output_len) ||
      !tc_internal_ranges_disjoint(info, count * sizeof *info, output, output_len))
    return TC_ERROR;
  total = 4 + z_len;
  for (i = 0; i < count; ++i) {
    if (!tc_internal_span_valid(info[i].data, info[i].length) ||
        info[i].length > SIZE_MAX - total ||
        !tc_internal_ranges_disjoint(info[i].data, info[i].length, output, output_len))
      return TC_ERROR;
    total += info[i].length;
  }
  /* Step 4 bounds counter || Z || FixedInfo by max_H_inputBits. The bound
   * used here, 2^64 - 1 bits, is the smallest in section 4.2 Table 1. */
#if SIZE_MAX > UINT64_MAX / 8
  if (total > UINT64_MAX / 8)
    return TC_ERROR;
#endif
  /* Step 2 rejects more than 2^32 - 1 blocks. */
#if SIZE_MAX > UINT32_MAX
  {
    size_t blocks = output_len / digest_length + (output_len % digest_length != 0);
    if (blocks > UINT32_MAX)
      return TC_ERROR;
  }
#endif
  while (offset < output_len) {
    const TC_bytes prefix[] = {{counter, sizeof counter}, secret};
    tc_internal_store_be32(counter, round);
    if (tc_hash_core_init(hash, ctx) != TC_OK ||
        tc_hash_core_update_parts(hash, ctx, prefix, sizeof prefix / sizeof *prefix) != TC_OK ||
        tc_hash_core_update_parts(hash, ctx, info, count) != TC_OK)
      goto done;
    if (tc_hash_core_final(hash, ctx, digest) != TC_OK)
      goto done;
    take = output_len - offset;
    if (take > digest_length)
      take = digest_length;
    memcpy(output + offset, digest, take);
    offset += take;
    ++round;
  }
  status = TC_OK;
done:
  TC_secure_zero(ctx, state->ctx_size);
  TC_secure_zero(digest, digest_length);
  if (status != TC_OK)
    TC_secure_zero(output, output_len);
  return status;
}

#define TC_SSKDF_FAMILY(N, BYTES)                                                                  \
  TC_status TC_SSKDF_SHA##N(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output)      \
  {                                                                                                \
    struct TC_SHA##N##_ctx ctx;                                                                    \
    uint8_t digest[BYTES];                                                                         \
    const sskdf_state state = {&ctx, sizeof ctx, digest};                                          \
    return derive(&tc_sha##N##_info, &state, z, info, count, output);                              \
  }
/* SP 800-56C Rev. 2 section 4.2 Table 1 approves each of these hashes for
 * Option 1. */
#if TC_ENABLE_SHA1
TC_SSKDF_FAMILY(1, TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_SHA224
TC_SSKDF_FAMILY(224, TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_SHA256
TC_SSKDF_FAMILY(256, TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_SHA384
TC_SSKDF_FAMILY(384, TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_SHA512
TC_SSKDF_FAMILY(512, TC_SHA512_DIGESTLEN)
#endif
#endif
