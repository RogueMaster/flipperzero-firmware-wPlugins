/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"

#if TC_ENABLE_DES && TC_DES_ENABLE_CTR
static const uint8_t key[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
static TC_status ctr(size_t length)
{
  static uint8_t buffer[16384];
  static const uint8_t iv[8] = {0};
  struct TC_DES_ctx ctx;
  TC_status status = TC_DES_init(&ctx, (TC_bytes){key, TC_DES_KEYLEN});
  if (status == TC_OK)
    status = TC_DES_set_iv(&ctx, (TC_bytes){iv, TC_DES_BLOCKLEN});
  if (status == TC_OK && length != 0)
    status = TC_DES_CTR_crypt(&ctx, (TC_buffer){buffer, length});
  if (status == TC_OK)
    tc_benchmark_consume(length ? (const void*)buffer : (const void*)&ctx);
  TC_DES_ctx_clear(&ctx);
  return status;
}
#endif
#if TC_ENABLE_DES && TC_DES_ENABLE_CTR && TC_DES_ENABLE_TDES
static const uint8_t key3[24] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                                 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
                                 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23};
static TC_status ctr3(size_t length)
{
  static uint8_t buffer[16384];
  static const uint8_t iv[8] = {0};
  struct TC_DES_ctx ctx;
  TC_status status = TC_DES_init(&ctx, (TC_bytes){key3, sizeof(key3)});
  if (status == TC_OK)
    status = TC_DES_set_iv(&ctx, (TC_bytes){iv, TC_DES_BLOCKLEN});
  if (status == TC_OK && length != 0)
    status = TC_DES_CTR_crypt(&ctx, (TC_buffer){buffer, length});
  if (status == TC_OK)
    tc_benchmark_consume(length ? (const void*)buffer : (const void*)&ctx);
  TC_DES_ctx_clear(&ctx);
  return status;
}
#endif
int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_DES && TC_DES_ENABLE_CTR
  if (tc_benchmark_run("DES setup+clear", 0, ctr) ||
      tc_benchmark_sizes("DES-CTR setup+encrypt+clear", ctr))
    return 1;
#endif
#if TC_ENABLE_DES && TC_DES_ENABLE_CTR && TC_DES_ENABLE_TDES
  if (tc_benchmark_run("TDEA setup+clear", 0, ctr3) ||
      tc_benchmark_sizes("TDEA-CTR setup+encrypt+clear", ctr3))
    return 1;
#endif
  return 0;
}
