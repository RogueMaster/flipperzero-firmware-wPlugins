/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"

#if TC_ENABLE_KDF
static const uint8_t key[32] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
  17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
};
static const uint8_t fixed[34] = {0};
static const struct TC_KBKDF_params params = {32, 1, 0};
#endif
#define TC_BENCH_KDF(NAME, KEYLEN) \
static TC_status NAME##_counter(size_t length) \
{ \
  uint8_t output[1024]; \
  TC_status status = TC_KBKDF_##NAME##_counter(key, KEYLEN, &params, \
      NULL, 0, fixed, sizeof(fixed), output, length); \
  if (status == TC_OK) tc_benchmark_consume(output); \
  return status; \
} \
static TC_status NAME##_feedback(size_t length) \
{ \
  uint8_t output[1024]; \
  TC_status status = TC_KBKDF_##NAME##_feedback(key, KEYLEN, &params, \
      NULL, 0, fixed, sizeof(fixed), output, length); \
  if (status == TC_OK) tc_benchmark_consume(output); \
  return status; \
} \
static TC_status NAME##_pipeline(size_t length) \
{ \
  uint8_t output[1024]; \
  TC_status status = TC_KBKDF_##NAME##_pipeline(key, KEYLEN, &params, \
      fixed, sizeof(fixed), output, length); \
  if (status == TC_OK) tc_benchmark_consume(output); \
  return status; \
}

#if TC_KBKDF_HAVE_HMAC_SHA1
TC_BENCH_KDF(HMAC_SHA1, sizeof(key))
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
TC_BENCH_KDF(HMAC_SHA224, sizeof(key))
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
TC_BENCH_KDF(HMAC_SHA256, sizeof(key))
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
TC_BENCH_KDF(HMAC_SHA384, sizeof(key))
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
TC_BENCH_KDF(HMAC_SHA512, sizeof(key))
#endif
#if TC_KBKDF_HAVE_AES_CMAC
TC_BENCH_KDF(AES_CMAC, TC_AES_KEYLEN)
#endif
#if TC_KBKDF_HAVE_DES_CMAC
TC_BENCH_KDF(DES_CMAC, 24)
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_KBKDF_HAVE_HMAC_SHA1
  if (tc_benchmark_run("KBKDF HMAC_SHA1 counter", 32, HMAC_SHA1_counter) ||
      tc_benchmark_run("KBKDF HMAC_SHA1 counter", 1024, HMAC_SHA1_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA1
  if (tc_benchmark_run("KBKDF HMAC_SHA1 feedback", 32, HMAC_SHA1_feedback) ||
      tc_benchmark_run("KBKDF HMAC_SHA1 feedback", 1024, HMAC_SHA1_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA1
  if (tc_benchmark_run("KBKDF HMAC_SHA1 pipeline", 32, HMAC_SHA1_pipeline) ||
      tc_benchmark_run("KBKDF HMAC_SHA1 pipeline", 1024, HMAC_SHA1_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
  if (tc_benchmark_run("KBKDF HMAC_SHA224 counter", 32, HMAC_SHA224_counter) ||
      tc_benchmark_run("KBKDF HMAC_SHA224 counter", 1024, HMAC_SHA224_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
  if (tc_benchmark_run("KBKDF HMAC_SHA224 feedback", 32, HMAC_SHA224_feedback) ||
      tc_benchmark_run("KBKDF HMAC_SHA224 feedback", 1024, HMAC_SHA224_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
  if (tc_benchmark_run("KBKDF HMAC_SHA224 pipeline", 32, HMAC_SHA224_pipeline) ||
      tc_benchmark_run("KBKDF HMAC_SHA224 pipeline", 1024, HMAC_SHA224_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
  if (tc_benchmark_run("KBKDF HMAC_SHA256 counter", 32, HMAC_SHA256_counter) ||
      tc_benchmark_run("KBKDF HMAC_SHA256 counter", 1024, HMAC_SHA256_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
  if (tc_benchmark_run("KBKDF HMAC_SHA256 feedback", 32, HMAC_SHA256_feedback) ||
      tc_benchmark_run("KBKDF HMAC_SHA256 feedback", 1024, HMAC_SHA256_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
  if (tc_benchmark_run("KBKDF HMAC_SHA256 pipeline", 32, HMAC_SHA256_pipeline) ||
      tc_benchmark_run("KBKDF HMAC_SHA256 pipeline", 1024, HMAC_SHA256_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
  if (tc_benchmark_run("KBKDF HMAC_SHA384 counter", 32, HMAC_SHA384_counter) ||
      tc_benchmark_run("KBKDF HMAC_SHA384 counter", 1024, HMAC_SHA384_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
  if (tc_benchmark_run("KBKDF HMAC_SHA384 feedback", 32, HMAC_SHA384_feedback) ||
      tc_benchmark_run("KBKDF HMAC_SHA384 feedback", 1024, HMAC_SHA384_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
  if (tc_benchmark_run("KBKDF HMAC_SHA384 pipeline", 32, HMAC_SHA384_pipeline) ||
      tc_benchmark_run("KBKDF HMAC_SHA384 pipeline", 1024, HMAC_SHA384_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
  if (tc_benchmark_run("KBKDF HMAC_SHA512 counter", 32, HMAC_SHA512_counter) ||
      tc_benchmark_run("KBKDF HMAC_SHA512 counter", 1024, HMAC_SHA512_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
  if (tc_benchmark_run("KBKDF HMAC_SHA512 feedback", 32, HMAC_SHA512_feedback) ||
      tc_benchmark_run("KBKDF HMAC_SHA512 feedback", 1024, HMAC_SHA512_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
  if (tc_benchmark_run("KBKDF HMAC_SHA512 pipeline", 32, HMAC_SHA512_pipeline) ||
      tc_benchmark_run("KBKDF HMAC_SHA512 pipeline", 1024, HMAC_SHA512_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_AES_CMAC
  if (tc_benchmark_run("KBKDF AES_CMAC counter", 32, AES_CMAC_counter) ||
      tc_benchmark_run("KBKDF AES_CMAC counter", 1024, AES_CMAC_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_AES_CMAC
  if (tc_benchmark_run("KBKDF AES_CMAC feedback", 32, AES_CMAC_feedback) ||
      tc_benchmark_run("KBKDF AES_CMAC feedback", 1024, AES_CMAC_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_AES_CMAC
  if (tc_benchmark_run("KBKDF AES_CMAC pipeline", 32, AES_CMAC_pipeline) ||
      tc_benchmark_run("KBKDF AES_CMAC pipeline", 1024, AES_CMAC_pipeline)) return 1;
#endif
#if TC_KBKDF_HAVE_DES_CMAC
  if (tc_benchmark_run("KBKDF DES_CMAC counter", 32, DES_CMAC_counter) ||
      tc_benchmark_run("KBKDF DES_CMAC counter", 1024, DES_CMAC_counter)) return 1;
#endif
#if TC_KBKDF_HAVE_DES_CMAC
  if (tc_benchmark_run("KBKDF DES_CMAC feedback", 32, DES_CMAC_feedback) ||
      tc_benchmark_run("KBKDF DES_CMAC feedback", 1024, DES_CMAC_feedback)) return 1;
#endif
#if TC_KBKDF_HAVE_DES_CMAC
  if (tc_benchmark_run("KBKDF DES_CMAC pipeline", 32, DES_CMAC_pipeline) ||
      tc_benchmark_run("KBKDF DES_CMAC pipeline", 1024, DES_CMAC_pipeline)) return 1;
#endif
  return 0;
}
