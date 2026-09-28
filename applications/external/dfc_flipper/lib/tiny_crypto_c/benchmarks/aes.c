/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"

#if TC_ENABLE_AES
static const uint8_t key[TC_AES_KEYLEN] = {0};
static TC_status setup(size_t unused)
{
  struct TC_AES_key_ctx ctx;
  TC_status status = TC_AES_key_init(&ctx, key);
  (void)unused;
  if (status == TC_OK) tc_benchmark_consume(ctx.round_key);
  TC_AES_key_ctx_clear(&ctx);
  return status;
}
#if TC_AES_ENABLE_CTR
static TC_status ctr(size_t length)
{
  static uint8_t buffer[16384];
  static const uint8_t iv[16] = {0};
  struct TC_AES_ctx ctx;
  TC_status status = TC_AES_init_ctx_iv(&ctx, key, iv);
  if (status == TC_OK) status = TC_AES_CTR_crypt(&ctx, buffer, length);
  tc_benchmark_consume(buffer);
  TC_AES_ctx_clear(&ctx);
  return status;
}
#endif
#if TC_AES_ENABLE_GCM
static TC_status gcm(size_t length)
{
  static uint8_t buffer[16384];
  static const uint8_t iv[12] = {0}, aad[16] = {0};
  uint8_t tag[16];
  TC_status status = TC_AES_GCM_encrypt(key, iv, sizeof(iv), aad, sizeof(aad),
                                        buffer, length, buffer, tag, sizeof(tag));
  if (status == TC_OK) tc_benchmark_consume(tag);
  return status;
}
#endif
#endif
int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_AES
  if (tc_benchmark_run("AES key setup+clear", 0, setup)) return 1;
#if TC_AES_ENABLE_CTR
  if (tc_benchmark_sizes("AES-CTR setup+encrypt+clear", ctr)) return 1;
#endif
#if TC_AES_ENABLE_GCM
  if (tc_benchmark_sizes("AES-GCM encrypt", gcm)) return 1;
#endif
#else
  puts("AES disabled");
#endif
  return 0;
}
