/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#include "des.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef BENCHMARK_BYTES
#define BENCHMARK_BYTES 16384u
#endif

#ifndef BENCHMARK_ITERATIONS
#define BENCHMARK_ITERATIONS 100u
#endif

#if !DES_ENABLE_CTR
#error "benchmark requires DES_ENABLE_CTR=1"
#endif

int main(void)
{
  static const uint8_t des_key[8] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
  };
  static const uint8_t tdes_key[24] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
    0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23
  };
  static const uint8_t iv0[8] = { 0 };
  static uint8_t buffer[BENCHMARK_BYTES];
  clock_t start;
  clock_t end;
  unsigned iteration;

  memset(buffer, 0xa5, sizeof(buffer));

  start = clock();
  for (iteration = 0; iteration < BENCHMARK_ITERATIONS; ++iteration)
  {
    struct DES_ctx ctx;
    DES_init_ctx_iv(&ctx, des_key, iv0);
    if (DES_CTR_crypt(&ctx, buffer, sizeof(buffer)) != DES_OK)
      return 1;
    DES_ctx_clear(&ctx);
  }
  end = clock();
  printf("DES-CTR bytes=%u iterations=%u seconds=%.6f bytes/sec=%.0f\n",
         (unsigned)sizeof(buffer), BENCHMARK_ITERATIONS,
         (double)(end - start) / (double)CLOCKS_PER_SEC,
         (double)sizeof(buffer) * BENCHMARK_ITERATIONS *
           (double)CLOCKS_PER_SEC / (double)(end - start));

#if DES_ENABLE_TDES
  start = clock();
  for (iteration = 0; iteration < BENCHMARK_ITERATIONS; ++iteration)
  {
    struct DES3_ctx ctx;
    if (DES3_init_ctx_iv(&ctx, tdes_key, sizeof(tdes_key), iv0) != DES_OK)
      return 1;
    if (DES3_CTR_crypt(&ctx, buffer, sizeof(buffer)) != DES_OK)
      return 1;
    DES3_ctx_clear(&ctx);
  }
  end = clock();
  printf("TDES-CTR bytes=%u iterations=%u seconds=%.6f bytes/sec=%.0f\n",
         (unsigned)sizeof(buffer), BENCHMARK_ITERATIONS,
         (double)(end - start) / (double)CLOCKS_PER_SEC,
         (double)sizeof(buffer) * BENCHMARK_ITERATIONS *
           (double)CLOCKS_PER_SEC / (double)(end - start));
#endif

  return 0;
}
