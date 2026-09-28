/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/sskdf.h>
#include "internal.h"
#include <string.h>
#if TC_ENABLE_SSKDF
#include <tiny_crypto/hash.h>

typedef struct {
  size_t digest_size, context_size;
  TC_status (*init)(void*);
  TC_status (*update)(void*, const uint8_t*, size_t);
  TC_status (*final)(void*, uint8_t*);
} tc_sskdf_hash;

static TC_status derive(const tc_sskdf_hash* hash, void* ctx,
    const uint8_t* z, size_t z_len, const TC_bytes* info, size_t count,
    uint8_t* output, size_t output_len)
{
  uint8_t counter[4], digest[48];
  size_t total, i, offset = 0, take;
  uint32_t round = 1;
  TC_status status = TC_ERROR;
  if (!z || !z_len || !output || !output_len || (!info && count) ||
      count > SIZE_MAX / sizeof *info || z_len > SIZE_MAX - 4 ||
      !tc_internal_ranges_disjoint(z, z_len, output, output_len) ||
      !tc_internal_ranges_disjoint(info, count * sizeof *info, output, output_len)) return TC_ERROR;
  total = 4 + z_len;
  for (i = 0; i < count; ++i) {
    if ((!info[i].data && info[i].length) || info[i].length > SIZE_MAX - total ||
        !tc_internal_ranges_disjoint(info[i].data, info[i].length, output, output_len)) return TC_ERROR;
    total += info[i].length;
  }
#if SIZE_MAX > UINT64_MAX / 8
  if (total > UINT64_MAX / 8) return TC_ERROR;
#endif
#if SIZE_MAX > UINT32_MAX
  {
    size_t blocks = output_len / hash->digest_size + (output_len % hash->digest_size != 0);
    if (blocks > UINT32_MAX) return TC_ERROR;
  }
#endif
  while (offset < output_len) {
    counter[0] = (uint8_t)(round >> 24); counter[1] = (uint8_t)(round >> 16);
    counter[2] = (uint8_t)(round >> 8); counter[3] = (uint8_t)round;
    if (hash->init(ctx) != TC_OK || hash->update(ctx, counter, 4) != TC_OK ||
        hash->update(ctx, z, z_len) != TC_OK) goto done;
    for (i = 0; i < count; ++i)
      if (hash->update(ctx, info[i].data, info[i].length) != TC_OK) goto done;
    if (hash->final(ctx, digest) != TC_OK) goto done;
    take = output_len - offset;
    if (take > hash->digest_size) take = hash->digest_size;
    memcpy(output + offset, digest, take);
    offset += take;
    ++round;
  }
  status = TC_OK;
done:
  TC_secure_zero(ctx, hash->context_size);
  TC_secure_zero(digest, sizeof digest);
  if (status != TC_OK) TC_secure_zero(output, output_len);
  return status;
}

#define TC_SSKDF_FAMILY(N, BYTES) \
  static TC_status init_##N(void* c) { return TC_SHA##N##_init((struct TC_SHA##N##_ctx*)c); } \
  static TC_status update_##N(void* c, const uint8_t* p, size_t n) \
  { return TC_SHA##N##_update((struct TC_SHA##N##_ctx*)c, p, n); } \
  static TC_status final_##N(void* c, uint8_t* p) { return TC_SHA##N##_final((struct TC_SHA##N##_ctx*)c, p); } \
  TC_status TC_SSKDF_SHA##N(const uint8_t* z, size_t z_len, const TC_bytes* info, size_t count, \
                          uint8_t* output, size_t output_len) \
  { \
    struct TC_SHA##N##_ctx ctx; \
    static const tc_sskdf_hash hash = {BYTES, sizeof ctx, init_##N, update_##N, final_##N}; \
    return derive(&hash, &ctx, z, z_len, info, count, output, output_len); \
  }
#if TC_ENABLE_SHA256
TC_SSKDF_FAMILY(256, 32)
#endif
#if TC_ENABLE_SHA384
TC_SSKDF_FAMILY(384, 48)
#endif
#endif
