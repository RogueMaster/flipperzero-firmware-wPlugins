/*
 * SPDX-License-Identifier: Unlicense
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 *
 * Unit tests for tiny-AES-c using munit: https://nemequ.github.io/munit/
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "aes.h"
#include "munit.h"
#include "test_vectors.h"
#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
#include "gcm_test_vectors.h"
#endif

#if defined(AES256) && (AES256 == 1)
#define TEST_KEY aes256_key
#define TEST_ECB_CIPHERTEXT aes256_ecb_ciphertext
#define TEST_CBC_CIPHERTEXT aes256_cbc_ciphertext
#define TEST_CTR_CIPHERTEXT aes256_ctr_ciphertext
#define TEST_OFB_CIPHERTEXT aes256_ofb_ciphertext
#elif defined(AES192) && (AES192 == 1)
#define TEST_KEY aes192_key
#define TEST_ECB_CIPHERTEXT aes192_ecb_ciphertext
#define TEST_CBC_CIPHERTEXT aes192_cbc_ciphertext
#define TEST_CTR_CIPHERTEXT aes192_ctr_ciphertext
#define TEST_OFB_CIPHERTEXT aes192_ofb_ciphertext
#else
#define TEST_KEY aes128_key
#define TEST_ECB_CIPHERTEXT aes128_ecb_ciphertext
#define TEST_CBC_CIPHERTEXT aes128_cbc_ciphertext
#define TEST_CTR_CIPHERTEXT aes128_ctr_ciphertext
#define TEST_OFB_CIPHERTEXT aes128_ofb_ciphertext
#endif

#ifndef CAVP_VECTOR_DIR
#define CAVP_VECTOR_DIR "test_vectors/cavp"
#endif

#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
static void test_initialize_sbox(void)
{
  AES_init_sbox();
}
#else
static void test_initialize_sbox(void)
{
}
#endif

#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1) && \
    AES_GCM_GHASH_MODE == AES_GCM_GHASH_MODE_HARDWARE
/* Test-only reference hook for validating the hardware dispatch path. */
void AES_CAVP_GHASH_HARDWARE_MULTIPLY(uint8_t* result,
                                      const uint8_t* left,
                                      const uint8_t* right)
{
  uint8_t value[AES_BLOCKLEN];
  uint8_t product[AES_BLOCKLEN] = { 0 };
  unsigned byte;
  unsigned bit;

  memcpy(value, right, AES_BLOCKLEN);
  for (byte = 0; byte < AES_BLOCKLEN; ++byte)
  {
    for (bit = 0; bit < 8; ++bit)
    {
      const uint8_t mask = (uint8_t)(0u - ((left[byte] >> (7u - bit)) & 1u));
      const uint8_t lsb = (uint8_t)(0u - (value[AES_BLOCKLEN - 1u] & 1u));
      unsigned i;

      for (i = 0; i < AES_BLOCKLEN; ++i)
        product[i] ^= (uint8_t)(value[i] & mask);
      for (i = AES_BLOCKLEN - 1u; i > 0; --i)
        value[i] = (uint8_t)((value[i] >> 1) | (value[i - 1u] << 7));
      value[0] = (uint8_t)((value[0] >> 1) ^ (0xe1u & lsb));
    }
  }
  memcpy(result, product, AES_BLOCKLEN);
}
#endif

#if defined(AES_CAVP) && (AES_CAVP == 1)
MunitResult test_cavp(const MunitParameter params[], void* data);
#endif
#if defined(AES_ENABLE_EAX) && (AES_ENABLE_EAX == 1)
MunitResult test_eax(const MunitParameter params[], void* data);
#endif
#if defined(AES_ENABLE_EAX_PRIME) && (AES_ENABLE_EAX_PRIME == 1)
MunitResult test_eax_prime(const MunitParameter params[], void* data);
#endif
#if defined(AES_ENABLE_SIV) && (AES_ENABLE_SIV == 1)
MunitResult test_siv(const MunitParameter params[], void* data);
#endif
#if defined(AES_ENABLE_CMAC) && (AES_ENABLE_CMAC == 1)
MunitResult test_cmac(const MunitParameter params[], void* data);
#endif

static MunitResult test_key_schedule(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx(&ctx, TEST_KEY);
  return MUNIT_OK;
}

static MunitResult test_secure_zero_and_clear(const MunitParameter params[],
                                              void* data)
{
  struct AES_ctx ctx;
  uint8_t buffer[32];
  size_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();
  for (i = 0; i < sizeof(buffer); ++i)
    buffer[i] = (uint8_t)(0xa5u ^ (uint8_t)i);
  AES_secure_zero(buffer, sizeof(buffer));
  for (i = 0; i < sizeof(buffer); ++i)
    munit_assert_uint8(buffer[i], ==, 0);

  AES_init_ctx(&ctx, TEST_KEY);
  AES_ctx_clear(&ctx);
  for (i = 0; i < sizeof(ctx); ++i)
    munit_assert_uint8(((const uint8_t*)&ctx)[i], ==, 0);

  return MUNIT_OK;
}

#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
#if defined(AES256) && (AES256 == 1)
#define TEST_GCM_VECTOR gcm_test_vectors[2]
#elif defined(AES192) && (AES192 == 1)
#define TEST_GCM_VECTOR gcm_test_vectors[1]
#else
#define TEST_GCM_VECTOR gcm_test_vectors[0]
#endif
#endif

#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
static MunitResult test_ecb(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t buffer[AES_BLOCKLEN];

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx(&ctx, TEST_KEY);
  memcpy(buffer, nist_plaintext, AES_BLOCKLEN);
  AES_ECB_encrypt(&ctx, buffer);
  munit_assert_memory_equal(AES_BLOCKLEN, buffer, TEST_ECB_CIPHERTEXT);

  AES_ECB_decrypt(&ctx, buffer);
  munit_assert_memory_equal(AES_BLOCKLEN, buffer, nist_plaintext);

  return MUNIT_OK;
}
#endif

#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
static MunitResult test_cbc(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t buffer[sizeof(nist_plaintext)];

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx_iv(&ctx, TEST_KEY, nist_iv);
  memcpy(buffer, nist_plaintext, sizeof(buffer));
  munit_assert_int(AES_CBC_encrypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, TEST_CBC_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_iv);
  munit_assert_int(AES_CBC_decrypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, nist_plaintext);

  return MUNIT_OK;
}

static MunitResult test_cbc_alignment(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t buffer[AES_BLOCKLEN + 1];

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx_iv(&ctx, TEST_KEY, nist_iv);
  memset(buffer, 0x5a, sizeof(buffer));
  munit_assert_int(AES_CBC_encrypt(&ctx, buffer, AES_BLOCKLEN + 1), ==, AES_ERR);
  munit_assert_int(AES_CBC_decrypt(&ctx, buffer, 1), ==, AES_ERR);
  munit_assert_int(AES_CBC_encrypt(&ctx, buffer, 0), ==, AES_OK);

  return MUNIT_OK;
}
#endif

#if defined(AES_ENABLE_CTR) && (AES_ENABLE_CTR == 1)
static MunitResult test_ctr(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t buffer[sizeof(nist_plaintext)];

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx_iv(&ctx, TEST_KEY, nist_ctr_iv);
  memcpy(buffer, nist_plaintext, sizeof(buffer));
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, TEST_CTR_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_ctr_iv);
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, nist_plaintext);

  return MUNIT_OK;
}

static MunitResult test_ctr_unaligned(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t storage[sizeof(nist_plaintext) + 1];
  uint8_t* buffer = storage + 1;

  (void) params;
  (void) data;

  test_initialize_sbox();
  AES_init_ctx_iv(&ctx, TEST_KEY, nist_ctr_iv);
  memcpy(buffer, nist_plaintext, sizeof(nist_plaintext));
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, sizeof(nist_plaintext)), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(nist_plaintext), buffer,
                            TEST_CTR_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_ctr_iv);
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, sizeof(nist_plaintext)), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(nist_plaintext), buffer, nist_plaintext);

  return MUNIT_OK;
}

static MunitResult test_ctr_wrap(const MunitParameter params[], void* data)
{
  struct AES_ctx ctx;
  uint8_t iv[AES_BLOCKLEN];
  uint8_t saved_iv[AES_BLOCKLEN];
  uint8_t buffer[AES_BLOCKLEN * 2];
  uint8_t saved_buffer[AES_BLOCKLEN * 2];
  uint8_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();
  memset(iv, 0xff, sizeof(iv));
  memset(buffer, 0x11, sizeof(buffer));
  memcpy(saved_buffer, buffer, sizeof(buffer));
  AES_init_ctx_iv(&ctx, TEST_KEY, iv);
  memcpy(saved_iv, ctx.Iv, sizeof(saved_iv));

  /* Two blocks from an all-0xff counter would wrap past 2^128. */
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, sizeof(buffer)), ==, AES_ERR);
  munit_assert_memory_equal(sizeof(buffer), buffer, saved_buffer);
  munit_assert_memory_equal(sizeof(saved_iv), ctx.Iv, saved_iv);

  /* A single block at max counter is allowed; IV then wraps to zero. */
  munit_assert_int(AES_CTR_crypt(&ctx, buffer, AES_BLOCKLEN), ==, AES_OK);
  for (i = 0; i < AES_BLOCKLEN; ++i)
    munit_assert_uint8(ctx.Iv[i], ==, 0);

  return MUNIT_OK;
}
#endif

#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
static MunitResult test_ofb(const MunitParameter params[], void* data)
{
  static const size_t encrypt_chunks[] = { 1, 15, 17, 31 };
  static const size_t decrypt_chunks[] = { 7, 9, 16, 32 };
  struct AES_ctx ctx;
  uint8_t buffer[sizeof(nist_plaintext)];
  uint8_t storage[sizeof(nist_plaintext) + 1];
  uint8_t* unaligned = storage + 1;
  size_t offset;
  size_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();

  AES_init_ctx_iv(&ctx, TEST_KEY, nist_iv);
  memcpy(buffer, nist_plaintext, sizeof(buffer));
  munit_assert_int(AES_OFB_crypt(&ctx, buffer, 0), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, nist_plaintext);
  munit_assert_int(AES_OFB_crypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, TEST_OFB_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_iv);
  munit_assert_int(AES_OFB_crypt(&ctx, buffer, sizeof(buffer)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(buffer), buffer, nist_plaintext);

  AES_ctx_set_iv(&ctx, nist_iv);
  memcpy(buffer, nist_plaintext, sizeof(buffer));
  offset = 0;
  for (i = 0; i < sizeof(encrypt_chunks) / sizeof(encrypt_chunks[0]); ++i)
  {
    munit_assert_int(AES_OFB_crypt(&ctx, buffer + offset, encrypt_chunks[i]),
                     ==, AES_OK);
    offset += encrypt_chunks[i];
  }
  munit_assert_memory_equal(sizeof(buffer), buffer, TEST_OFB_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_iv);
  offset = 0;
  for (i = 0; i < sizeof(decrypt_chunks) / sizeof(decrypt_chunks[0]); ++i)
  {
    munit_assert_int(AES_OFB_crypt(&ctx, buffer + offset, decrypt_chunks[i]),
                     ==, AES_OK);
    offset += decrypt_chunks[i];
  }
  munit_assert_memory_equal(sizeof(buffer), buffer, nist_plaintext);

  AES_ctx_set_iv(&ctx, nist_iv);
  memcpy(unaligned, nist_plaintext, sizeof(nist_plaintext));
  munit_assert_int(AES_OFB_crypt(&ctx, unaligned, 37), ==, AES_OK);
  munit_assert_memory_equal(37, unaligned, TEST_OFB_CIPHERTEXT);

  AES_ctx_set_iv(&ctx, nist_iv);
  munit_assert_int(AES_OFB_crypt(&ctx, unaligned, 37), ==, AES_OK);
  munit_assert_memory_equal(37, unaligned, nist_plaintext);

  return MUNIT_OK;
}
#endif

#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)
#if !defined(AES192) && !defined(AES256)
static void test_ccm_vector(const uint8_t* key, const uint8_t* nonce,
                            size_t nonce_len, const uint8_t* aad,
                            size_t aad_len, const uint8_t* plaintext,
                            size_t plaintext_len, const uint8_t* ciphertext,
                            const uint8_t* tag, size_t tag_len)
{
  uint8_t buffer[64] = { 0 };
  uint8_t output[64] = { 0 };
  uint8_t generated_tag[16] = { 0 };

  memcpy(buffer, plaintext, plaintext_len);
  munit_assert_int(AES_CCM_encrypt(key, nonce, nonce_len, aad, aad_len,
                                   buffer, plaintext_len, output,
                                   generated_tag, tag_len), ==,
                   AES_OK);
  munit_assert_memory_equal(plaintext_len, output, ciphertext);
  munit_assert_memory_equal(tag_len, generated_tag, tag);

  memset(buffer, 0, sizeof(buffer));
  munit_assert_int(AES_CCM_decrypt(key, nonce, nonce_len, aad, aad_len,
                                   output, plaintext_len, tag, tag_len,
                                   buffer), ==, AES_OK);
  munit_assert_memory_equal(plaintext_len, buffer, plaintext);

  generated_tag[0] ^= 1;
  memset(buffer, 0xa5, sizeof(buffer));
  munit_assert_int(AES_CCM_decrypt(key, nonce, nonce_len, aad, aad_len,
                                   output, plaintext_len, generated_tag,
                                   tag_len, buffer), ==, AES_ERR);
  for (size_t i = 0; i < plaintext_len; ++i)
    munit_assert_uint8(buffer[i], ==, 0);
}

static MunitResult test_ccm(const MunitParameter params[], void* data)
{
  static uint8_t nist_aad4[65536];
  size_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();
  for (i = 0; i < sizeof(nist_aad4); ++i)
    nist_aad4[i] = (uint8_t)i;
  test_ccm_vector(ccm_nist_key, ccm_nist_nonce1, sizeof(ccm_nist_nonce1),
                  ccm_nist_aad1, sizeof(ccm_nist_aad1), ccm_nist_plaintext1,
                  sizeof(ccm_nist_plaintext1), ccm_nist_ciphertext1,
                  ccm_nist_tag1, sizeof(ccm_nist_tag1));
  test_ccm_vector(ccm_nist_key, ccm_nist_nonce2, sizeof(ccm_nist_nonce2),
                  ccm_nist_aad2, sizeof(ccm_nist_aad2), ccm_nist_plaintext2,
                  sizeof(ccm_nist_plaintext2), ccm_nist_ciphertext2,
                  ccm_nist_tag2, sizeof(ccm_nist_tag2));
  test_ccm_vector(ccm_nist_key, ccm_nist_nonce4, sizeof(ccm_nist_nonce4),
                  nist_aad4, sizeof(nist_aad4), ccm_nist_plaintext4,
                  sizeof(ccm_nist_plaintext4), ccm_nist_ciphertext4,
                  ccm_nist_tag4, sizeof(ccm_nist_tag4));
  test_ccm_vector(ccm_rfc_key, ccm_rfc_nonce, sizeof(ccm_rfc_nonce),
                  ccm_rfc_aad, sizeof(ccm_rfc_aad), ccm_rfc_plaintext,
                  sizeof(ccm_rfc_plaintext), ccm_rfc_ciphertext, ccm_rfc_tag,
                  sizeof(ccm_rfc_tag));
  return MUNIT_OK;
}

static MunitResult test_ccm_api(const MunitParameter params[], void* data)
{
  static uint8_t large_aad[65280];
  static uint8_t max_plaintext[65535];
  static uint8_t max_ciphertext[65535];
  static uint8_t max_plaintext_copy[65535];
  static const uint8_t nonce13[13] = { 0 };
  uint8_t buffer[64] = { 0 };
  uint8_t ciphertext[64] = { 0 };
  uint8_t tag[16] = { 0 };
  uint8_t bad_aad[sizeof(ccm_nist_aad1)];
  uint8_t bad_nonce[sizeof(ccm_nist_nonce1)];
  uint8_t bad_tag[sizeof(ccm_nist_tag1)];
  uint8_t one = 0;
  size_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();
  memcpy(bad_aad, ccm_nist_aad1, sizeof(bad_aad));
  memcpy(bad_nonce, ccm_nist_nonce1, sizeof(bad_nonce));
  memcpy(bad_tag, ccm_nist_tag1, sizeof(bad_tag));

  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1, 6,
                                   ccm_nist_aad1, sizeof(ccm_nist_aad1),
                                   ccm_nist_plaintext1,
                                   sizeof(ccm_nist_plaintext1), ciphertext,
                                   tag, sizeof(ccm_nist_tag1)), ==, AES_ERR);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1, 14,
                                   ccm_nist_aad1, sizeof(ccm_nist_aad1),
                                   ccm_nist_plaintext1,
                                   sizeof(ccm_nist_plaintext1), ciphertext,
                                   tag, sizeof(ccm_nist_tag1)), ==, AES_ERR);
  for (i = 0; i <= 18; ++i)
  {
    if (i == 4 || i == 6 || i == 8 || i == 10 || i == 12 || i == 14 || i == 16)
      continue;
    munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                     sizeof(ccm_nist_nonce1), ccm_nist_aad1,
                                     sizeof(ccm_nist_aad1), ccm_nist_plaintext1,
                                     sizeof(ccm_nist_plaintext1), ciphertext,
                                     tag, i), ==, AES_ERR);
  }

  munit_assert_int(AES_CCM_encrypt(NULL, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), NULL, 0, NULL, 0,
                                   NULL, tag, sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, NULL,
                                   sizeof(ccm_nist_nonce1), NULL, 0, NULL, 0,
                                   NULL, tag, sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), ccm_nist_aad1, 1,
                                   NULL, 0, NULL, tag, sizeof(tag)), ==, AES_OK);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), NULL, 1, NULL, 0,
                                   NULL, tag, sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), NULL, 0, &one, 1,
                                   NULL, tag, sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), NULL, 0, &one, 1,
                                   ciphertext, NULL, sizeof(tag)), ==, AES_ERR);

  memcpy(buffer, ccm_nist_plaintext1, sizeof(ccm_nist_plaintext1));
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), ccm_nist_aad1,
                                   sizeof(ccm_nist_aad1), buffer,
                                   sizeof(ccm_nist_plaintext1), buffer, tag,
                                   sizeof(ccm_nist_tag1)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(ccm_nist_ciphertext1), buffer,
                            ccm_nist_ciphertext1);
  munit_assert_memory_equal(sizeof(ccm_nist_tag1), tag, ccm_nist_tag1);
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), ccm_nist_aad1,
                                   sizeof(ccm_nist_aad1), buffer,
                                   sizeof(ccm_nist_plaintext1), tag,
                                   sizeof(ccm_nist_tag1), buffer), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(ccm_nist_plaintext1), buffer,
                            ccm_nist_plaintext1);

  memcpy(ciphertext, ccm_nist_ciphertext1, sizeof(ccm_nist_ciphertext1));
  ciphertext[0] ^= 1;
  memset(buffer, 0xa5, sizeof(buffer));
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), ccm_nist_aad1,
                                   sizeof(ccm_nist_aad1), ciphertext,
                                   sizeof(ccm_nist_ciphertext1),
                                   ccm_nist_tag1, sizeof(ccm_nist_tag1),
                                   buffer), ==, AES_ERR);
  for (i = 0; i < sizeof(ccm_nist_plaintext1); ++i)
    munit_assert_uint8(buffer[i], ==, 0);

  bad_tag[0] ^= 1;
  memset(buffer, 0xa5, sizeof(buffer));
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), ccm_nist_aad1,
                                   sizeof(ccm_nist_aad1),
                                   ccm_nist_ciphertext1,
                                   sizeof(ccm_nist_ciphertext1), bad_tag,
                                   sizeof(bad_tag), buffer), ==, AES_ERR);
  bad_aad[0] ^= 1;
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, ccm_nist_nonce1,
                                   sizeof(ccm_nist_nonce1), bad_aad,
                                   sizeof(bad_aad), ccm_nist_ciphertext1,
                                   sizeof(ccm_nist_ciphertext1), ccm_nist_tag1,
                                   sizeof(ccm_nist_tag1), buffer), ==,
                   AES_ERR);
  bad_nonce[0] ^= 1;
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, bad_nonce, sizeof(bad_nonce),
                                   ccm_nist_aad1, sizeof(ccm_nist_aad1),
                                   ccm_nist_ciphertext1,
                                   sizeof(ccm_nist_ciphertext1), ccm_nist_tag1,
                                   sizeof(ccm_nist_tag1), buffer), ==,
                   AES_ERR);

  for (i = 0; i < sizeof(large_aad); ++i)
    large_aad[i] = (uint8_t)i;
  for (i = 0; i < 2; ++i)
  {
    const size_t aad_len = i == 0 ? 65279u : 65280u;
    munit_assert_int(AES_CCM_encrypt(ccm_nist_key, ccm_nist_nonce1,
                                     sizeof(ccm_nist_nonce1), large_aad,
                                     aad_len, ccm_nist_plaintext1,
                                     sizeof(ccm_nist_plaintext1), ciphertext,
                                     tag, sizeof(ccm_nist_tag1)), ==,
                     AES_OK);
    munit_assert_int(AES_CCM_decrypt(ccm_nist_key, ccm_nist_nonce1,
                                     sizeof(ccm_nist_nonce1), large_aad,
                                     aad_len, ciphertext,
                                     sizeof(ccm_nist_plaintext1), tag,
                                     sizeof(ccm_nist_tag1), buffer), ==,
                     AES_OK);
    munit_assert_memory_equal(sizeof(ccm_nist_plaintext1), buffer,
                              ccm_nist_plaintext1);
  }

  memset(max_plaintext, 0x5a, sizeof(max_plaintext));
  memcpy(max_plaintext_copy, max_plaintext, sizeof(max_plaintext));
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, nonce13, sizeof(nonce13),
                                   NULL, 0, max_plaintext,
                                   sizeof(max_plaintext), max_ciphertext, tag,
                                   sizeof(ccm_nist_tag1)), ==, AES_OK);
  munit_assert_int(AES_CCM_decrypt(ccm_nist_key, nonce13, sizeof(nonce13),
                                   NULL, 0, max_ciphertext,
                                   sizeof(max_ciphertext), tag,
                                   sizeof(ccm_nist_tag1), max_plaintext), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(max_plaintext), max_plaintext,
                            max_plaintext_copy);
  munit_assert_int(AES_CCM_encrypt(ccm_nist_key, nonce13, sizeof(nonce13),
                                   NULL, 0, &one, sizeof(max_plaintext) + 1,
                                   max_ciphertext, tag,
                                   sizeof(ccm_nist_tag1)), ==, AES_ERR);
  for (i = 7; i <= 13; ++i)
  {
    const unsigned q = (unsigned)(15 - i);
    const uint64_t maximum = q == sizeof(uint64_t) ? UINT64_MAX :
                             ((UINT64_C(1) << (8u * q)) - 1u);
    if (maximum < (uint64_t)SIZE_MAX)
    {
      const size_t over = (size_t)(maximum + 1u);
      munit_assert_int(AES_CCM_encrypt(ccm_nist_key, nonce13, i, NULL, 0,
                                       &one, over, ciphertext, tag,
                                       sizeof(ccm_nist_tag1)), ==,
                       AES_ERR);
    }
  }

  return MUNIT_OK;
}

#endif

#endif

#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
static MunitResult test_gcm(const MunitParameter params[], void* data)
{
  const struct gcm_test_vector* vector = &TEST_GCM_VECTOR;
  struct AES_GCM_ctx ctx;
  uint8_t buffer[16];
  uint8_t tag[16];
  uint8_t short_tag[4];
  uint8_t bad_tag[16];

  (void) params;
  (void) data;

  test_initialize_sbox();
  memcpy(buffer, vector->plaintext, vector->length);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, 5), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad + 5, vector->aad_len - 5), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer, 3), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer + 3, vector->length - 3), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_finish(&ctx, tag), ==, AES_OK);
  munit_assert_memory_equal(vector->length, buffer, vector->ciphertext);
  munit_assert_memory_equal(vector->tag_len, tag, vector->tag);

  /* Fixed t=4 for this key/context; MSBt of the 128-bit tag. */
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len, 4),
                   ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, vector->aad_len), ==, AES_OK);
  memcpy(buffer, vector->plaintext, vector->length);
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer, vector->length), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_finish(&ctx, short_tag), ==, AES_OK);
  munit_assert_memory_equal(sizeof(short_tag), short_tag, vector->tag);

  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, vector->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_update(&ctx, buffer, vector->length), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_finish(&ctx, tag), ==, AES_OK);
  munit_assert_memory_equal(vector->length, buffer, vector->plaintext);

  memcpy(buffer, vector->ciphertext, vector->length);
  memcpy(bad_tag, tag, sizeof(bad_tag));
  bad_tag[0] ^= 1;
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, vector->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_update(&ctx, buffer, vector->length), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_finish(&ctx, bad_tag), ==, AES_ERR);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, 1), ==, AES_ERR);

  /* Reject invalid tag lengths at init (SP 800-38D). */
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len, 0),
                   ==, AES_ERR);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len, 5),
                   ==, AES_ERR);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len, 17),
                   ==, AES_ERR);

  return MUNIT_OK;
}

static MunitResult test_gcm_direction(const MunitParameter params[], void* data)
{
  const struct gcm_test_vector* vector = &TEST_GCM_VECTOR;
  struct AES_GCM_ctx ctx;
  uint8_t buffer[16];
  uint8_t before[16];

  (void) params;
  (void) data;

  test_initialize_sbox();
  memcpy(buffer, vector->plaintext, vector->length);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer, 1), ==, AES_OK);
  memcpy(before, buffer, sizeof(before));
  munit_assert_int(AES_GCM_decrypt_update(&ctx, buffer, 1), ==, AES_ERR);
  munit_assert_memory_equal(sizeof(before), buffer, before);

  memcpy(buffer, vector->ciphertext, vector->length);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_update(&ctx, buffer, 1), ==, AES_OK);
  memcpy(before, buffer, sizeof(before));
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer, 1), ==, AES_ERR);
  munit_assert_memory_equal(sizeof(before), buffer, before);

  memcpy(buffer, vector->ciphertext, vector->length);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, vector->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_update(&ctx, buffer, vector->length), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_finish(&ctx, (const uint8_t*)vector->tag),
                   ==, AES_OK);
  munit_assert_memory_equal(vector->length, buffer, vector->plaintext);

  return MUNIT_OK;
}

#if !defined(AES192) && !defined(AES256)
static MunitResult test_gcm_non96_iv(const MunitParameter params[], void* data)
{
  const struct gcm_test_vector* vector = &gcm_non96_test_vector;
  struct AES_GCM_ctx ctx;
  uint8_t buffer[16];
  uint8_t tag[16];

  (void) params;
  (void) data;

  test_initialize_sbox();
  memcpy(buffer, vector->plaintext, vector->length);
  munit_assert_int(AES_GCM_init(&ctx, vector->key, vector->iv, vector->iv_len,
                                vector->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx, vector->aad, vector->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx, buffer, vector->length), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_finish(&ctx, tag), ==, AES_OK);
  munit_assert_memory_equal(vector->length, buffer, vector->ciphertext);
  munit_assert_memory_equal(vector->tag_len, tag, vector->tag);

  return MUNIT_OK;
}
#endif

static MunitResult test_gcm_oneshot(const MunitParameter params[], void* data)
{
  const struct gcm_test_vector* vector = &TEST_GCM_VECTOR;
  uint8_t ciphertext[64];
  uint8_t plaintext[64];
  uint8_t tag[16];
  uint8_t bad_tag[16];
  uint8_t poison[64];
  uint8_t overlap[64];
  size_t i;

  (void) params;
  (void) data;

  test_initialize_sbox();
  munit_assert_size(vector->length, <=, sizeof(ciphertext));

  munit_assert_int(AES_GCM_encrypt(vector->key, vector->iv, vector->iv_len,
                                   vector->aad, vector->aad_len,
                                   vector->plaintext, vector->length,
                                   ciphertext, tag, vector->tag_len), ==,
                   AES_OK);
  munit_assert_memory_equal(vector->length, ciphertext, vector->ciphertext);
  munit_assert_memory_equal(vector->tag_len, tag, vector->tag);

  memset(plaintext, 0xa5, sizeof(plaintext));
  munit_assert_int(AES_GCM_decrypt(vector->key, vector->iv, vector->iv_len,
                                   vector->aad, vector->aad_len,
                                   ciphertext, vector->length, tag,
                                   vector->tag_len, plaintext), ==, AES_OK);
  munit_assert_memory_equal(vector->length, plaintext, vector->plaintext);

  /* Bad tag must not write plaintext (non-aliasing buffers). */
  memcpy(bad_tag, tag, vector->tag_len);
  bad_tag[0] ^= 1u;
  memset(poison, 0x3c, sizeof(poison));
  munit_assert_int(AES_GCM_decrypt(vector->key, vector->iv, vector->iv_len,
                                   vector->aad, vector->aad_len,
                                   ciphertext, vector->length, bad_tag,
                                   vector->tag_len, poison), ==, AES_ERR);
  for (i = 0; i < vector->length; ++i)
    munit_assert_uint8(poison[i], ==, 0x3c);

  /* In-place decrypt with bad tag zeros the shared buffer. */
  memcpy(ciphertext, vector->ciphertext, vector->length);
  munit_assert_int(AES_GCM_decrypt(vector->key, vector->iv, vector->iv_len,
                                   vector->aad, vector->aad_len,
                                   ciphertext, vector->length, bad_tag,
                                   vector->tag_len, ciphertext), ==, AES_ERR);
  for (i = 0; i < vector->length; ++i)
    munit_assert_uint8(ciphertext[i], ==, 0);

  /* Partial overlap is rejected before any write. */
  memcpy(overlap, vector->plaintext, vector->length);
  munit_assert_int(AES_GCM_encrypt(vector->key, vector->iv, vector->iv_len,
                                   vector->aad, vector->aad_len,
                                   overlap, vector->length,
                                   overlap + 1, tag, vector->tag_len), ==,
                   AES_ERR);
  munit_assert_memory_equal(vector->length, overlap, vector->plaintext);

  return MUNIT_OK;
}

#if defined(GCM_MULTI_KEY_TEST) && (GCM_MULTI_KEY_TEST == 1)
/* Two contexts, two keys: both must work independently (per-context state). */
static MunitResult test_gcm_multi_key(const MunitParameter params[], void* data)
{
  static const uint8_t key_b[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
  };
  const struct gcm_test_vector* va = &TEST_GCM_VECTOR;
  struct AES_GCM_ctx ctx_a;
  struct AES_GCM_ctx ctx_b;
  uint8_t buf_a[32];
  uint8_t buf_b[32];
  uint8_t tag_a[16];
  uint8_t tag_b[16];
  uint8_t expect_b[32];

  (void) params;
  (void) data;

  test_initialize_sbox();
  munit_assert_size(va->length, <=, sizeof(buf_a));

  memcpy(buf_a, va->plaintext, va->length);
  munit_assert_int(AES_GCM_init(&ctx_a, va->key, va->iv, va->iv_len,
                                va->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx_a, va->aad, va->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx_a, buf_a, va->length), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_finish(&ctx_a, tag_a), ==, AES_OK);

  memcpy(buf_b, va->plaintext, va->length);
  munit_assert_int(AES_GCM_init(&ctx_b, key_b, va->iv, va->iv_len,
                                va->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx_b, va->aad, va->aad_len), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_update(&ctx_b, buf_b, va->length), ==, AES_OK);
  munit_assert_int(AES_GCM_encrypt_finish(&ctx_b, tag_b), ==, AES_OK);

  /* Different keys must not produce the same ciphertext/tag as the vector. */
  munit_assert_memory_equal(va->length, buf_a, va->ciphertext);
  munit_assert_memory_equal(va->tag_len, tag_a, va->tag);
  munit_assert_memory_not_equal(va->length, buf_b, va->ciphertext);

  /* Context A remains valid after B was initialized with another key. */
  memcpy(expect_b, buf_b, va->length);
  munit_assert_int(AES_GCM_init(&ctx_a, va->key, va->iv, va->iv_len,
                                va->tag_len), ==, AES_OK);
  munit_assert_int(AES_GCM_aad_update(&ctx_a, va->aad, va->aad_len), ==, AES_OK);
  memcpy(buf_a, va->ciphertext, va->length);
  munit_assert_int(AES_GCM_decrypt_update(&ctx_a, buf_a, va->length), ==, AES_OK);
  munit_assert_int(AES_GCM_decrypt_finish(&ctx_a, tag_a), ==, AES_OK);
  munit_assert_memory_equal(va->length, buf_a, va->plaintext);
  munit_assert_memory_equal(va->length, buf_b, expect_b);

  AES_GCM_clear(&ctx_a);
  AES_GCM_clear(&ctx_b);
  return MUNIT_OK;
}
#endif
#endif

static MunitTest test_suite_tests[] = {
  /*
   * Mode suites that require AES_init_sbox() (runtime profile) are listed
   * before /key-schedule. munit does not fork on Windows (MUNIT_NO_FORK), so
   * an earlier test that initializes the process-global S-box would mask a
   * missing AES_init_sbox() in a later test. On Unix, each test is forked and
   * gets a fresh BSS, so the same bug fails there immediately.
   */
#if defined(AES_ENABLE_SIV) && (AES_ENABLE_SIV == 1)
  { "/siv", test_siv, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_CMAC) && (AES_ENABLE_CMAC == 1)
  { "/cmac", test_cmac, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_EAX) && (AES_ENABLE_EAX == 1)
  { "/eax", test_eax, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_EAX_PRIME) && (AES_ENABLE_EAX_PRIME == 1)
  { "/eax-prime", test_eax_prime, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
  { "/key-schedule", test_key_schedule, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/secure-zero-clear", test_secure_zero_and_clear, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
  { "/ecb", test_ecb, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
  { "/cbc", test_cbc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/cbc-alignment", test_cbc_alignment, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_CTR) && (AES_ENABLE_CTR == 1)
  { "/ctr", test_ctr, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/ctr-unaligned", test_ctr_unaligned, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/ctr-wrap", test_ctr_wrap, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
  { "/ofb", test_ofb, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_CAVP) && (AES_CAVP == 1) && \
    ((defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
     (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)) || (defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)) || \
     (defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)))
  { "/cavp", test_cavp, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)
#if !defined(AES192) && !defined(AES256)
  { "/ccm", test_ccm, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/ccm-api", test_ccm_api, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#endif
#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
  { "/gcm", test_gcm, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/gcm-direction", test_gcm_direction, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { "/gcm-oneshot", test_gcm_oneshot, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#if !defined(AES192) && !defined(AES256)
  { "/gcm-non96-iv", test_gcm_non96_iv, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#if defined(GCM_MULTI_KEY_TEST) && (GCM_MULTI_KEY_TEST == 1)
  { "/gcm-multi-key", test_gcm_multi_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
#endif
#endif
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite test_suite = {
  "/tiny-aes-c",
  test_suite_tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[])
{
  return munit_suite_main(&test_suite, NULL, argc, argv);
}
