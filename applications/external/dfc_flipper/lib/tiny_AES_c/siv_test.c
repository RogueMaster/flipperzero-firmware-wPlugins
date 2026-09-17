/*
 * SPDX-License-Identifier: Unlicense
 * SPDX-FileCopyrightText: Mistial Dev
 *
 * AES-SIV tests: RFC 5297 Appendix A and Wycheproof AEAD-AES-SIV-CMAC.
 * Test-only translation unit.
 */

#include "aes.h"
#include "munit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SIV_VECTOR_FILE
#define SIV_VECTOR_FILE "test_vectors/siv/aead_aes_siv_cmac_test.json"
#endif

#if defined(AES_ENABLE_SIV) && (AES_ENABLE_SIV == 1)

#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
static void siv_initialize_sbox(void)
{
  AES_init_sbox();
}
#else
static void siv_initialize_sbox(void)
{
}
#endif

static int siv_hex_value(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static size_t siv_decode_hex(const char* text, uint8_t* output, size_t capacity)
{
  size_t length = 0;
  while (*text != '\0')
  {
    const int high = siv_hex_value((unsigned char)*text++);
    int low;
    if (high < 0)
      continue;
    if (*text == '\0')
      return SIZE_MAX;
    low = siv_hex_value((unsigned char)*text++);
    if (low < 0 || length == capacity)
      return SIZE_MAX;
    output[length++] = (uint8_t)((high << 4) | low);
  }
  return length;
}

/* RFC 5297 Appendix A.1 — deterministic AEAD (AES-SIV-CMAC-256). */
#if !defined(AES192) && !defined(AES256)
static MunitResult test_siv_rfc_a1(const MunitParameter params[], void* data)
{
  static const uint8_t key[AES_SIV_KEYLEN] = {
    0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
    0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
  };
  static const uint8_t ad_bytes[] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
  };
  static const uint8_t plaintext[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee
  };
  static const uint8_t expect_v[AES_SIV_V_LEN] = {
    0x85, 0x63, 0x2d, 0x07, 0xc6, 0xe8, 0xf3, 0x7f,
    0x95, 0x0a, 0xcd, 0x32, 0x0a, 0x2e, 0xcc, 0x93
  };
  static const uint8_t expect_c[] = {
    0x40, 0xc0, 0x2b, 0x96, 0x90, 0xc4, 0xdc, 0x04,
    0xda, 0xef, 0x7f, 0x6a, 0xfe, 0x5c
  };
  const uint8_t* ad[1];
  size_t ad_lens[1];
  uint8_t v[AES_SIV_V_LEN];
  uint8_t ct[sizeof(plaintext)];
  uint8_t pt[sizeof(plaintext)];

  (void) params;
  (void) data;

  ad[0] = ad_bytes;
  ad_lens[0] = sizeof(ad_bytes);

  munit_assert_int(AES_SIV_encrypt(key, ad, ad_lens, 1, plaintext,
                                   sizeof(plaintext), v, ct), ==, AES_OK);
  munit_assert_memory_equal(AES_SIV_V_LEN, v, expect_v);
  munit_assert_memory_equal(sizeof(expect_c), ct, expect_c);

  munit_assert_int(AES_SIV_decrypt(key, ad, ad_lens, 1, v, ct,
                                   sizeof(ct), pt), ==, AES_OK);
  munit_assert_memory_equal(sizeof(plaintext), pt, plaintext);

  v[0] ^= 1u;
  memset(pt, 0xa5, sizeof(pt));
  munit_assert_int(AES_SIV_decrypt(key, ad, ad_lens, 1, v, ct,
                                   sizeof(ct), pt), ==, AES_ERR);
  {
    size_t i;
    for (i = 0; i < sizeof(pt); ++i)
      munit_assert_uint8(pt[i], ==, 0);
  }

  return MUNIT_OK;
}

/* RFC 5297 Appendix A.2 — nonce-based AEAD (AD1, AD2, Nonce, P). */
static MunitResult test_siv_rfc_a2(const MunitParameter params[], void* data)
{
  static const uint8_t key[AES_SIV_KEYLEN] = {
    0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x7a, 0x79, 0x78,
    0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f
  };
  static const uint8_t ad1[] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0xde, 0xad, 0xda, 0xda, 0xde, 0xad, 0xda, 0xda,
    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
  };
  static const uint8_t ad2[] = {
    0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0
  };
  static const uint8_t nonce[] = {
    0x09, 0xf9, 0x11, 0x02, 0x9d, 0x74, 0xe3, 0x5b,
    0xd8, 0x41, 0x56, 0xc5, 0x63, 0x56, 0x88, 0xc0
  };
  static const uint8_t plaintext[] = {
    0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20,
    0x73, 0x6f, 0x6d, 0x65, 0x20, 0x70, 0x6c, 0x61,
    0x69, 0x6e, 0x74, 0x65, 0x78, 0x74, 0x20, 0x74,
    0x6f, 0x20, 0x65, 0x6e, 0x63, 0x72, 0x79, 0x70,
    0x74, 0x20, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x20,
    0x53, 0x49, 0x56, 0x2d, 0x41, 0x45, 0x53
  };
  static const uint8_t expect_v[AES_SIV_V_LEN] = {
    0x7b, 0xdb, 0x6e, 0x3b, 0x43, 0x26, 0x67, 0xeb,
    0x06, 0xf4, 0xd1, 0x4b, 0xff, 0x2f, 0xbd, 0x0f
  };
  static const uint8_t expect_c[] = {
    0xcb, 0x90, 0x0f, 0x2f, 0xdd, 0xbe, 0x40, 0x43,
    0x26, 0x60, 0x19, 0x65, 0xc8, 0x89, 0xbf, 0x17,
    0xdb, 0xa7, 0x7c, 0xeb, 0x09, 0x4f, 0xa6, 0x63,
    0xb7, 0xa3, 0xf7, 0x48, 0xba, 0x8a, 0xf8, 0x29,
    0xea, 0x64, 0xad, 0x54, 0x4a, 0x27, 0x2e, 0x9c,
    0x48, 0x5b, 0x62, 0xa3, 0xfd, 0x5c, 0x0d
  };
  const uint8_t* ad[3];
  size_t ad_lens[3];
  uint8_t v[AES_SIV_V_LEN];
  uint8_t ct[sizeof(plaintext)];
  uint8_t pt[sizeof(plaintext)];

  (void) params;
  (void) data;

  ad[0] = ad1;
  ad_lens[0] = sizeof(ad1);
  ad[1] = ad2;
  ad_lens[1] = sizeof(ad2);
  ad[2] = nonce;
  ad_lens[2] = sizeof(nonce);

  munit_assert_int(AES_SIV_encrypt(key, ad, ad_lens, 3, plaintext,
                                   sizeof(plaintext), v, ct), ==, AES_OK);
  munit_assert_memory_equal(AES_SIV_V_LEN, v, expect_v);
  munit_assert_memory_equal(sizeof(expect_c), ct, expect_c);

  munit_assert_int(AES_SIV_decrypt(key, ad, ad_lens, 3, v, ct,
                                   sizeof(ct), pt), ==, AES_OK);
  munit_assert_memory_equal(sizeof(plaintext), pt, plaintext);

  return MUNIT_OK;
}
#endif /* AES128 */

/*
 * Wycheproof AEAD-AES-SIV-CMAC: S2V AD vector is (aad, nonce=iv) then
 * plaintext. Empty aad is still passed as a zero-length component.
 * Only groups whose key size matches this build's AES_SIV_KEYLEN run.
 */
static MunitResult test_siv_wycheproof(const MunitParameter params[], void* data)
{
  FILE* file;
  char line[2048];
  char key_hex[256];
  char iv_hex[128];
  char aad_hex[4096];
  char msg_hex[8192];
  char ct_hex[8192];
  char tag_hex[64];
  char result[16];
  int key_size_bits = 0;
  int in_group = 0;
  int have = 0;
  unsigned ran = 0;
  unsigned ran_valid = 0;
  unsigned ran_invalid = 0;
  unsigned failed = 0;
  /* Vendored corpus: 300 cases per SIV key size (84 valid + 216 invalid). */
  const unsigned expect_total = 300u;
  const unsigned expect_valid = 84u;
  const unsigned expect_invalid = 216u;
  const int want_bits = (int)(AES_SIV_KEYLEN * 8);

  (void) params;
  (void) data;

  key_hex[0] = iv_hex[0] = aad_hex[0] = msg_hex[0] = ct_hex[0] = tag_hex[0] =
    result[0] = '\0';

  file = fopen(SIV_VECTOR_FILE, "rb");
  munit_assert_not_null(file);

  while (fgets(line, (int)sizeof(line), file) != NULL)
  {
    char* p = line;
    while (*p == ' ' || *p == '\t')
      ++p;

    if (strncmp(p, "\"keySize\"", 9) == 0)
    {
      char* colon = strchr(p, ':');
      if (colon != NULL)
        key_size_bits = (int)strtol(colon + 1, NULL, 10);
      in_group = (key_size_bits == want_bits);
      continue;
    }

    if (!in_group)
      continue;

    if (strncmp(p, "\"key\":", 6) == 0 || strncmp(p, "\"iv\":", 5) == 0 ||
        strncmp(p, "\"aad\":", 6) == 0 || strncmp(p, "\"msg\":", 6) == 0 ||
        strncmp(p, "\"ct\":", 5) == 0 || strncmp(p, "\"tag\":", 6) == 0 ||
        strncmp(p, "\"result\":", 9) == 0)
    {
      char* q1 = strchr(p, ':');
      char* q2;
      char* q3;
      char* dest = NULL;
      size_t dest_sz = 0;
      if (q1 == NULL)
        continue;
      q2 = strchr(q1, '"');
      if (q2 == NULL)
        continue;
      ++q2;
      q3 = strchr(q2, '"');
      if (q3 == NULL)
        continue;
      if (strncmp(p, "\"key\":", 6) == 0) { dest = key_hex; dest_sz = sizeof(key_hex); }
      else if (strncmp(p, "\"iv\":", 5) == 0) { dest = iv_hex; dest_sz = sizeof(iv_hex); }
      else if (strncmp(p, "\"aad\":", 6) == 0) { dest = aad_hex; dest_sz = sizeof(aad_hex); }
      else if (strncmp(p, "\"msg\":", 6) == 0) { dest = msg_hex; dest_sz = sizeof(msg_hex); }
      else if (strncmp(p, "\"ct\":", 5) == 0) { dest = ct_hex; dest_sz = sizeof(ct_hex); }
      else if (strncmp(p, "\"tag\":", 6) == 0) { dest = tag_hex; dest_sz = sizeof(tag_hex); }
      else { dest = result; dest_sz = sizeof(result); }
      {
        size_t n = (size_t)(q3 - q2);
        if (n >= dest_sz)
          n = dest_sz - 1;
        memcpy(dest, q2, n);
        dest[n] = '\0';
      }
      have |= 1;
      if (strncmp(p, "\"result\"", 8) == 0 && have)
      {
        uint8_t key[64];
        uint8_t iv[64];
        uint8_t aad[2048];
        uint8_t msg[4096];
        uint8_t ct[4096];
        uint8_t tag[AES_SIV_V_LEN];
        uint8_t out_ct[4096];
        uint8_t out_pt[4096];
        uint8_t out_v[AES_SIV_V_LEN];
        size_t key_len, iv_len, aad_len, msg_len, ct_len, tag_len;
        const uint8_t* ad[2];
        size_t ad_lens[2];
        int expect_ok = (strcmp(result, "valid") == 0);

        key_len = siv_decode_hex(key_hex, key, sizeof(key));
        iv_len = siv_decode_hex(iv_hex, iv, sizeof(iv));
        aad_len = siv_decode_hex(aad_hex, aad, sizeof(aad));
        msg_len = siv_decode_hex(msg_hex, msg, sizeof(msg));
        ct_len = siv_decode_hex(ct_hex, ct, sizeof(ct));
        tag_len = siv_decode_hex(tag_hex, tag, sizeof(tag));

        if (key_len == SIZE_MAX || iv_len == SIZE_MAX || aad_len == SIZE_MAX ||
            msg_len == SIZE_MAX || ct_len == SIZE_MAX || tag_len == SIZE_MAX ||
            key_len != AES_SIV_KEYLEN || tag_len != AES_SIV_V_LEN ||
            msg_len != ct_len)
        {
          failed++;
          have = 0;
          continue;
        }

        ad[0] = aad;
        ad_lens[0] = aad_len;
        ad[1] = iv;
        ad_lens[1] = iv_len;

        ++ran;
        if (expect_ok)
          ++ran_valid;
        else
          ++ran_invalid;

        if (expect_ok)
        {
          if (AES_SIV_encrypt(key, ad, ad_lens, 2, msg, msg_len, out_v,
                              out_ct) != AES_OK ||
              memcmp(out_v, tag, AES_SIV_V_LEN) != 0 ||
              memcmp(out_ct, ct, ct_len) != 0)
            ++failed;
          else if (AES_SIV_decrypt(key, ad, ad_lens, 2, tag, ct, ct_len,
                                   out_pt) != AES_OK ||
                   memcmp(out_pt, msg, msg_len) != 0)
            ++failed;
        }
        else
        {
          if (AES_SIV_decrypt(key, ad, ad_lens, 2, tag, ct, ct_len,
                              out_pt) != AES_ERR)
            ++failed;
        }
        have = 0;
        key_hex[0] = '\0';
      }
    }
  }

  fclose(file);
  munit_assert_uint(ran, ==, expect_total);
  munit_assert_uint(ran_valid, ==, expect_valid);
  munit_assert_uint(ran_invalid, ==, expect_invalid);
  munit_assert_uint(failed, ==, 0);
  return MUNIT_OK;
}

static MunitResult test_siv_api(const MunitParameter params[], void* data)
{
  uint8_t key[AES_SIV_KEYLEN];
  uint8_t v[AES_SIV_V_LEN];
  uint8_t v2[AES_SIV_V_LEN];
  uint8_t buf[32];
  uint8_t saved[32];
  uint8_t pt[16];
  uint8_t ct[16];
  uint8_t empty;
  const uint8_t* ad[AES_SIV_MAX_AD + 1u];
  size_t ad_lens[AES_SIV_MAX_AD + 1u];
  size_t i;

  (void) params;
  (void) data;

  memset(key, 0x11, sizeof(key));
  memset(buf, 0x22, sizeof(buf));
  memset(pt, 0x33, sizeof(pt));

  /* NULL / bound checks */
  munit_assert_int(AES_SIV_encrypt(NULL, NULL, NULL, 0, buf, sizeof(buf), v,
                                   buf), ==, AES_ERR);
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 1, buf, sizeof(buf), v,
                                   buf), ==, AES_ERR);
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, AES_SIV_MAX_AD + 1, buf,
                                   sizeof(buf), v, buf), ==, AES_ERR);
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, buf, sizeof(buf), NULL,
                                   buf), ==, AES_ERR);

  /* Empty plaintext, no AD */
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, NULL, 0, v, NULL),
                   ==, AES_OK);
  munit_assert_int(AES_SIV_decrypt(key, NULL, NULL, 0, v, NULL, 0, NULL),
                   ==, AES_OK);

  /* Zero-length AD component vs no AD — both valid, different transcripts */
  ad[0] = &empty;
  ad_lens[0] = 0;
  munit_assert_int(AES_SIV_encrypt(key, ad, ad_lens, 1, pt, sizeof(pt), v, ct),
                   ==, AES_OK);
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, pt, sizeof(pt), v2, buf),
                   ==, AES_OK);
  munit_assert_memory_not_equal(AES_SIV_V_LEN, v, v2);

  /* 126 AD components accepted; 127 rejected */
  for (i = 0; i < AES_SIV_MAX_AD + 1u; ++i)
  {
    ad[i] = &empty;
    ad_lens[i] = 0;
  }
  munit_assert_int(AES_SIV_encrypt(key, ad, ad_lens, AES_SIV_MAX_AD, pt,
                                   sizeof(pt), v, ct), ==, AES_OK);
  munit_assert_int(AES_SIV_encrypt(key, ad, ad_lens, AES_SIV_MAX_AD + 1u, pt,
                                   sizeof(pt), v, ct), ==, AES_ERR);

  /* In-place encrypt/decrypt success */
  memcpy(buf, pt, sizeof(pt));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, buf, sizeof(pt), v, buf),
                   ==, AES_OK);
  munit_assert_int(AES_SIV_decrypt(key, NULL, NULL, 0, v, buf, sizeof(pt), buf),
                   ==, AES_OK);
  munit_assert_memory_equal(sizeof(pt), buf, pt);

  /* In-place decrypt failure wipes the buffer completely */
  memcpy(buf, pt, sizeof(pt));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, buf, sizeof(pt), v, buf),
                   ==, AES_OK);
  v[0] ^= 1u;
  munit_assert_int(AES_SIV_decrypt(key, NULL, NULL, 0, v, buf, sizeof(pt), buf),
                   ==, AES_ERR);
  for (i = 0; i < sizeof(pt); ++i)
    munit_assert_uint8(buf[i], ==, 0);
  v[0] ^= 1u;

  /* Partial pt/ct overlap rejected; buffers unchanged */
  memcpy(buf, pt, sizeof(pt));
  memcpy(saved, buf, sizeof(buf));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, buf, sizeof(pt), v,
                                   buf + 1), ==, AES_ERR);
  munit_assert_memory_equal(sizeof(buf), buf, saved);

  /* v may alias plaintext when ciphertext is distinct (staged); succeeds */
  memcpy(buf, pt, sizeof(pt));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, buf, sizeof(pt), buf, ct),
                   ==, AES_OK);
  /* first 16 bytes of buf are now V; decrypt with that V into pt-sized tail */
  {
    uint8_t rec[16];
    munit_assert_int(AES_SIV_decrypt(key, NULL, NULL, 0, buf, ct, sizeof(pt),
                                     rec), ==, AES_OK);
    munit_assert_memory_equal(sizeof(pt), rec, pt);
  }

  /* Exact v == ciphertext rejected; neither buffer written */
  memcpy(buf, pt, sizeof(pt));
  memcpy(saved, buf, sizeof(buf));
  memcpy(ct, pt, sizeof(pt)); /* sentinel; must stay if encrypt is rejected */
  {
    uint8_t ct_saved[16];
    memcpy(ct_saved, ct, sizeof(ct));
    munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, pt, sizeof(pt), buf,
                                     buf), ==, AES_ERR);
    munit_assert_memory_equal(sizeof(buf), buf, saved);
    munit_assert_memory_equal(sizeof(ct), ct, ct_saved);
  }

  /* Partial v/ciphertext overlap rejected; buffers unchanged */
  memcpy(buf, pt, sizeof(pt));
  memcpy(buf + 16, pt, sizeof(pt));
  memcpy(saved, buf, sizeof(buf));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, pt, sizeof(pt), buf,
                                   buf + 1), ==, AES_ERR);
  munit_assert_memory_equal(sizeof(buf), buf, saved);

  /* v fully after ciphertext (disjoint) is OK */
  memcpy(buf, pt, sizeof(pt));
  munit_assert_int(AES_SIV_encrypt(key, NULL, NULL, 0, pt, sizeof(pt), buf + 16,
                                   buf), ==, AES_OK);
  munit_assert_int(AES_SIV_decrypt(key, NULL, NULL, 0, buf + 16, buf,
                                   sizeof(pt), ct), ==, AES_OK);
  munit_assert_memory_equal(sizeof(pt), ct, pt);

  return MUNIT_OK;
}

MunitResult test_siv(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;

  siv_initialize_sbox();

#if !defined(AES192) && !defined(AES256)
  if (test_siv_rfc_a1(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_siv_rfc_a2(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
#endif
  if (test_siv_wycheproof(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_siv_api(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  return MUNIT_OK;
}

#else /* !SIV */

MunitResult test_siv(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;
  return MUNIT_SKIP;
}

#endif
