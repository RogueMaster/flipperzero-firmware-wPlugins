/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_BENCHMARK_SUPPORT_H
#define TC_BENCHMARK_SUPPORT_H

#include <tiny_crypto/tiny_crypto.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>

#ifndef TC_BENCHMARK_SECONDS
#define TC_BENCHMARK_SECONDS 0.1
#endif
#ifndef TC_BENCHMARK_BUILD_TYPE
#define TC_BENCHMARK_BUILD_TYPE "unknown"
#define TC_BENCHMARK_COMPILER "unknown"
#define TC_BENCHMARK_SANITIZE ""
#endif

void tc_benchmark_consume(const void* value);

static inline void tc_benchmark_profile(void)
{
  printf("compiler=%s build=%s sanitizer=%s AES=%d key_bits=%d sbox=%d "
         "ghash=%d wide=%d zeroize=%d strict=%d\n",
         TC_BENCHMARK_COMPILER, TC_BENCHMARK_BUILD_TYPE, TC_BENCHMARK_SANITIZE,
         TC_ENABLE_AES, TC_AES_KEY_BITS, TC_AES_SBOX_MODE,
         TC_AES_GCM_GHASH_MODE, TC_AES_WIDE_OPS, TC_ZEROIZE, TC_STRICT);
#if TC_ENABLE_AES && TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
  TC_AES_init_sbox();
#endif
}

/* An opaque result consumer keeps work even with whole-program optimization.
 * Batching amortizes clock overhead; setup/clear costs belong to each operation.
 * Repeated nonces below are synthetic benchmark inputs, never protocol traffic. */
static inline int tc_benchmark_run(const char* name, size_t bytes,
                                  TC_status (*operation)(size_t))
{
  unsigned long count = 0, batch = 1, i;
  clock_t start, end;
  double elapsed;
  if (operation(bytes) != TC_OK)
    return 1;
  start = clock();
  if (start == (clock_t)-1)
    return 1;
  do {
    for (i = 0; i < batch; ++i)
      if (operation(bytes) != TC_OK)
        return 1;
    if (count > ULONG_MAX - batch)
      return 1;
    count += batch;
    end = clock();
    if (end == (clock_t)-1 || end < start)
      return 1;
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    if (batch < 65536ul)
      batch *= 2;
  } while (elapsed < TC_BENCHMARK_SECONDS);
  if (elapsed <= 0)
    return 1;
  printf("%s bytes=%lu iterations=%lu seconds=%.6f ops/sec=%.0f bytes/sec=%.0f\n",
         name, (unsigned long)bytes, count, elapsed, count / elapsed,
         (double)bytes * count / elapsed);
  return 0;
}

static inline int tc_benchmark_sizes(const char* name,
                                    TC_status (*operation)(size_t))
{
  return tc_benchmark_run(name, 32, operation) ||
         tc_benchmark_run(name, 16384, operation);
}
#endif
