/*
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#include "aes.h"

#include <stdio.h>
#include <time.h>

#ifndef BENCHMARK_BYTES
#define BENCHMARK_BYTES 16384u
#endif

#ifndef BENCHMARK_ITERATIONS
#define BENCHMARK_ITERATIONS 100u
#endif

#ifndef AES_BENCHMARK_NOW
#define AES_BENCHMARK_NOW() clock()
#endif

#ifndef AES_BENCHMARK_TICKS_PER_SECOND
#define AES_BENCHMARK_TICKS_PER_SECOND ((double)CLOCKS_PER_SEC)
#endif

int main(void)
{
  static const uint8_t key[16] = { 0 };
  static const uint8_t iv[12] = { 0 };
  static const uint8_t aad[16] = { 0 };
  static uint8_t buffer[BENCHMARK_BYTES];
  uint8_t tag[16];
  clock_t start;
  clock_t end;
  unsigned iteration;

  start = AES_BENCHMARK_NOW();
  for (iteration = 0; iteration < BENCHMARK_ITERATIONS; ++iteration)
  {
    struct AES_GCM_ctx ctx;
    if (AES_GCM_init(&ctx, key, iv, sizeof(iv), sizeof(tag)) != AES_OK ||
        AES_GCM_aad_update(&ctx, aad, sizeof(aad)) != AES_OK ||
        AES_GCM_encrypt_update(&ctx, buffer, sizeof(buffer)) != AES_OK ||
        AES_GCM_encrypt_finish(&ctx, tag) != AES_OK)
      return 1;
    AES_GCM_clear(&ctx);
  }
  end = AES_BENCHMARK_NOW();

  printf("GCM profile=%d bytes=%u iterations=%u seconds=%.6f bytes/sec=%.0f\n",
         AES_GCM_GHASH_MODE, (unsigned)sizeof(buffer), BENCHMARK_ITERATIONS,
         (double)(end - start) / AES_BENCHMARK_TICKS_PER_SECOND,
         (double)sizeof(buffer) * BENCHMARK_ITERATIONS *
           AES_BENCHMARK_TICKS_PER_SECOND / (double)(end - start));
  return 0;
}
