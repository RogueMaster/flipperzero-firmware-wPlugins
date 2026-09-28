/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"

#define TC_BENCH_HASH(N) \
static TC_status hash##N(size_t length) \
{ \
  static const uint8_t input[16384] = {0}; \
  uint8_t output[TC_SHA##N##_DIGESTLEN]; \
  TC_status status = TC_SHA##N##_digest(input, length, output); \
  if (status == TC_OK) tc_benchmark_consume(output); \
  return status; \
} \
static TC_status init##N(size_t unused) \
{ \
  struct TC_SHA##N##_ctx ctx; \
  TC_status status = TC_SHA##N##_init(&ctx); \
  (void)unused; \
  tc_benchmark_consume(&ctx); \
  TC_SHA##N##_ctx_clear(&ctx); \
  return status; \
}

#define TC_BENCH_HMAC(N) \
static TC_status hmac##N(size_t length) \
{ \
  static const uint8_t input[16384] = {0}, key[32] = {0}; \
  uint8_t output[TC_SHA##N##_DIGESTLEN]; \
  TC_status status = TC_HMAC_SHA##N##_digest(key, sizeof(key), input, length, \
                                            output, sizeof(output)); \
  if (status == TC_OK) tc_benchmark_consume(output); \
  return status; \
} \
static TC_status hmac_init##N(size_t unused) \
{ \
  struct TC_HMAC_SHA##N##_ctx ctx; \
  static const uint8_t key[32] = {0}; \
  TC_status status = TC_HMAC_SHA##N##_init(&ctx, key, sizeof(key)); \
  (void)unused; \
  if (status == TC_OK) tc_benchmark_consume(&ctx); \
  TC_HMAC_SHA##N##_ctx_clear(&ctx); \
  return status; \
}

#if TC_ENABLE_SHA1
TC_BENCH_HASH(1)
#if TC_ENABLE_HMAC
TC_BENCH_HMAC(1)
#endif
#endif
#if TC_ENABLE_SHA224
TC_BENCH_HASH(224)
#if TC_ENABLE_HMAC
TC_BENCH_HMAC(224)
#endif
#endif
#if TC_ENABLE_SHA256
TC_BENCH_HASH(256)
#if TC_ENABLE_HMAC
TC_BENCH_HMAC(256)
#endif
#endif
#if TC_ENABLE_SHA384
TC_BENCH_HASH(384)
#if TC_ENABLE_HMAC
TC_BENCH_HMAC(384)
#endif
#endif
#if TC_ENABLE_SHA512
TC_BENCH_HASH(512)
#if TC_ENABLE_HMAC
TC_BENCH_HMAC(512)
#endif
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_SHA1
  if (tc_benchmark_run("SHA-1 setup+clear", 0, init1) ||
      tc_benchmark_sizes("SHA-1 digest", hash1)) return 1;
#if TC_ENABLE_HMAC
  if (tc_benchmark_run("HMAC-SHA-1 setup+clear", 0, hmac_init1) ||
      tc_benchmark_sizes("HMAC-SHA-1 digest", hmac1)) return 1;
#endif
#endif
#if TC_ENABLE_SHA224
  if (tc_benchmark_run("SHA-224 setup+clear", 0, init224) ||
      tc_benchmark_sizes("SHA-224 digest", hash224)) return 1;
#if TC_ENABLE_HMAC
  if (tc_benchmark_run("HMAC-SHA-224 setup+clear", 0, hmac_init224) ||
      tc_benchmark_sizes("HMAC-SHA-224 digest", hmac224)) return 1;
#endif
#endif
#if TC_ENABLE_SHA256
  if (tc_benchmark_run("SHA-256 setup+clear", 0, init256) ||
      tc_benchmark_sizes("SHA-256 digest", hash256)) return 1;
#if TC_ENABLE_HMAC
  if (tc_benchmark_run("HMAC-SHA-256 setup+clear", 0, hmac_init256) ||
      tc_benchmark_sizes("HMAC-SHA-256 digest", hmac256)) return 1;
#endif
#endif
#if TC_ENABLE_SHA384
  if (tc_benchmark_run("SHA-384 setup+clear", 0, init384) ||
      tc_benchmark_sizes("SHA-384 digest", hash384)) return 1;
#if TC_ENABLE_HMAC
  if (tc_benchmark_run("HMAC-SHA-384 setup+clear", 0, hmac_init384) ||
      tc_benchmark_sizes("HMAC-SHA-384 digest", hmac384)) return 1;
#endif
#endif
#if TC_ENABLE_SHA512
  if (tc_benchmark_run("SHA-512 setup+clear", 0, init512) ||
      tc_benchmark_sizes("SHA-512 digest", hash512)) return 1;
#if TC_ENABLE_HMAC
  if (tc_benchmark_run("HMAC-SHA-512 setup+clear", 0, hmac_init512) ||
      tc_benchmark_sizes("HMAC-SHA-512 digest", hmac512)) return 1;
#endif
#endif
  return 0;
}
