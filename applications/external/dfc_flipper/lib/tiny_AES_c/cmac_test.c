/*
 * SPDX-License-Identifier: Unlicense
 * SPDX-FileCopyrightText: Mistial Dev
 *
 * AES-CMAC tests: SP 800-38B Appendix D, NIST CAVP Gen/Ver, Wycheproof.
 * Test-only translation unit.
 */

#include "aes.h"
#include "munit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CMAC_WYCHEPROOF_FILE
#define CMAC_WYCHEPROOF_FILE "test_vectors/cmac/aes_cmac_test.json"
#endif

#ifndef CMAC_CAVP_DIR
#define CMAC_CAVP_DIR "test_vectors/cmac"
#endif

#if defined(AES_ENABLE_CMAC) && (AES_ENABLE_CMAC == 1)

#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
static void cmac_initialize_sbox(void)
{
  AES_init_sbox();
}
#else
static void cmac_initialize_sbox(void)
{
}
#endif

static int cmac_hex_value(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static size_t cmac_decode_hex(const char* text, uint8_t* output, size_t capacity)
{
  size_t length = 0;
  while (*text != '\0')
  {
    const int high = cmac_hex_value((unsigned char)*text++);
    int low;
    if (high < 0)
      continue;
    if (*text == '\0')
      return SIZE_MAX;
    low = cmac_hex_value((unsigned char)*text++);
    if (low < 0 || length == capacity)
      return SIZE_MAX;
    output[length++] = (uint8_t)((high << 4) | low);
  }
  return length;
}

/* NIST SP 800-38B Appendix D — full 16-byte tags. */
static MunitResult test_cmac_sp800_38b(const MunitParameter params[], void* data)
{
  uint8_t tag[AES_CMAC_TAG_MAX];

  (void) params;
  (void) data;

#if !defined(AES192) && !defined(AES256)
  {
    static const uint8_t key[16] = {
      0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
      0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };
    static const uint8_t m1[16] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    static const uint8_t m2[40] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
    };
    static const uint8_t m3[64] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
      0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
      0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
      0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
    };
    static const uint8_t t0[16] = {
      0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
      0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46
    };
    static const uint8_t t1[16] = {
      0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
      0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c
    };
    static const uint8_t t2[16] = {
      0xdf, 0xa6, 0x67, 0x47, 0xde, 0x9a, 0xe6, 0x30,
      0x30, 0xca, 0x32, 0x61, 0x14, 0x97, 0xc8, 0x27
    };
    static const uint8_t t3[16] = {
      0x51, 0xf0, 0xbe, 0xbf, 0x7e, 0x3b, 0x9d, 0x92,
      0xfc, 0x49, 0x74, 0x17, 0x79, 0x36, 0x3c, 0xfe
    };

    munit_assert_int(AES_CMAC(key, NULL, 0, tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t0);
    munit_assert_int(AES_CMAC_verify(key, NULL, 0, t0, 16), ==, AES_OK);

    munit_assert_int(AES_CMAC(key, m1, sizeof(m1), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t1);

    munit_assert_int(AES_CMAC(key, m2, sizeof(m2), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t2);

    munit_assert_int(AES_CMAC(key, m3, sizeof(m3), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t3);

    /* Truncation: leading octets of T */
    munit_assert_int(AES_CMAC(key, m1, sizeof(m1), tag, 8), ==, AES_OK);
    munit_assert_memory_equal(8, tag, t1);
    munit_assert_int(AES_CMAC_verify(key, m1, sizeof(m1), t1, 8), ==, AES_OK);
  }
#elif defined(AES192) && (AES192 == 1)
  {
    static const uint8_t key[24] = {
      0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
      0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
      0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
    };
    static const uint8_t m1[16] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    static const uint8_t m2[40] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
    };
    static const uint8_t m3[64] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
      0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
      0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
      0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
    };
    static const uint8_t t0[16] = {
      0xd1, 0x7d, 0xdf, 0x46, 0xad, 0xaa, 0xcd, 0xe5,
      0x31, 0xca, 0xc4, 0x83, 0xde, 0x7a, 0x93, 0x67
    };
    static const uint8_t t1[16] = {
      0x9e, 0x99, 0xa7, 0xbf, 0x31, 0xe7, 0x10, 0x90,
      0x06, 0x62, 0xf6, 0x5e, 0x61, 0x7c, 0x51, 0x84
    };
    static const uint8_t t2[16] = {
      0x8a, 0x1d, 0xe5, 0xbe, 0x2e, 0xb3, 0x1a, 0xad,
      0x08, 0x9a, 0x82, 0xe6, 0xee, 0x90, 0x8b, 0x0e
    };
    static const uint8_t t3[16] = {
      0xa1, 0xd5, 0xdf, 0x0e, 0xed, 0x79, 0x0f, 0x79,
      0x4d, 0x77, 0x58, 0x96, 0x59, 0xf3, 0x9a, 0x11
    };

    munit_assert_int(AES_CMAC(key, NULL, 0, tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t0);
    munit_assert_int(AES_CMAC(key, m1, sizeof(m1), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t1);
    munit_assert_int(AES_CMAC(key, m2, sizeof(m2), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t2);
    munit_assert_int(AES_CMAC(key, m3, sizeof(m3), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t3);
  }
#else /* AES256 */
  {
    static const uint8_t key[32] = {
      0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
      0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
      0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
      0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
    };
    static const uint8_t m1[16] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    static const uint8_t m2[40] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
    };
    static const uint8_t m3[64] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
      0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
      0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
      0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
    };
    static const uint8_t t0[16] = {
      0x02, 0x89, 0x62, 0xf6, 0x1b, 0x7b, 0xf8, 0x9e,
      0xfc, 0x6b, 0x55, 0x1f, 0x46, 0x67, 0xd9, 0x83
    };
    static const uint8_t t1[16] = {
      0x28, 0xa7, 0x02, 0x3f, 0x45, 0x2e, 0x8f, 0x82,
      0xbd, 0x4b, 0xf2, 0x8d, 0x8c, 0x37, 0xc3, 0x5c
    };
    static const uint8_t t2[16] = {
      0xaa, 0xf3, 0xd8, 0xf1, 0xde, 0x56, 0x40, 0xc2,
      0x32, 0xf5, 0xb1, 0x69, 0xb9, 0xc9, 0x11, 0xe6
    };
    static const uint8_t t3[16] = {
      0xe1, 0x99, 0x21, 0x90, 0x54, 0x9f, 0x6e, 0xd5,
      0x69, 0x6a, 0x2c, 0x05, 0x6c, 0x31, 0x54, 0x10
    };

    munit_assert_int(AES_CMAC(key, NULL, 0, tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t0);
    munit_assert_int(AES_CMAC(key, m1, sizeof(m1), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t1);
    munit_assert_int(AES_CMAC(key, m2, sizeof(m2), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t2);
    munit_assert_int(AES_CMAC(key, m3, sizeof(m3), tag, 16), ==, AES_OK);
    munit_assert_memory_equal(16, tag, t3);
  }
#endif

  return MUNIT_OK;
}

static MunitResult test_cmac_api(const MunitParameter params[], void* data)
{
  uint8_t key[AES_KEYLEN];
  uint8_t msg[16];
  uint8_t tag[AES_CMAC_TAG_MAX];
  uint8_t tag2[AES_CMAC_TAG_MAX];

  (void) params;
  (void) data;

  memset(key, 0x11, sizeof(key));
  memset(msg, 0x22, sizeof(msg));

  munit_assert_int(AES_CMAC(NULL, msg, sizeof(msg), tag, 16), ==, AES_ERR);
  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), NULL, 16), ==, AES_ERR);
  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), tag, 0), ==, AES_ERR);
  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), tag, 17), ==, AES_ERR);
  munit_assert_int(AES_CMAC(key, NULL, 1, tag, 16), ==, AES_ERR);
#if AES_CMAC_MIN_TAG_LEN > 1
  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), tag,
                            AES_CMAC_MIN_TAG_LEN - 1u), ==, AES_ERR);
  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), tag,
                                   AES_CMAC_MIN_TAG_LEN - 1u), ==, AES_ERR);
#endif

  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), tag, 16), ==, AES_OK);
  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), tag, 16), ==, AES_OK);

  tag[0] ^= 1u;
  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), tag, 16), ==, AES_ERR);
  tag[0] ^= 1u;

  /* Truncation at AES_CMAC_MIN_TAG_LEN (product default 8; CAVP builds use 4). */
  munit_assert_int(AES_CMAC(key, msg, sizeof(msg), tag2, AES_CMAC_MIN_TAG_LEN),
                   ==, AES_OK);
  munit_assert_memory_equal(AES_CMAC_MIN_TAG_LEN, tag2, tag);
  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), tag2,
                                   AES_CMAC_MIN_TAG_LEN), ==, AES_OK);

  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), NULL, 16), ==, AES_ERR);
  munit_assert_int(AES_CMAC_verify(key, msg, sizeof(msg), tag, 0), ==, AES_ERR);

  return MUNIT_OK;
}

/*
 * Full Wycheproof AES-CMAC corpus (311 cases).
 * Per key-size build: matching keySize groups (102) plus InvalidKeySize (5).
 * Matching: valid → generate+verify; invalid ModifiedTag → verify fails.
 * Wrong key length: API is fixed AES_KEYLEN; result must be invalid.
 */
static MunitResult test_cmac_wycheproof(const MunitParameter params[], void* data)
{
  FILE* file;
  char line[1024];
  char key_hex[128];
  char msg_hex[256];
  char tag_hex[64];
  char result[16];
  int key_size_bits = 0;
  int in_group = 0;
  int have = 0;
  unsigned ran = 0;
  unsigned ran_valid = 0;
  unsigned ran_invalid = 0;
  unsigned failed = 0;
  /* 102 matching this AES_KEYLEN + 5 InvalidKeySize shared across builds. */
  const unsigned expect_total = 107u;
  const unsigned expect_valid = 21u;
  const unsigned expect_invalid = 86u;
  const int want_bits = (int)(AES_KEYLEN * 8);

  (void) params;
  (void) data;

  key_hex[0] = msg_hex[0] = tag_hex[0] = result[0] = '\0';

  file = fopen(CMAC_WYCHEPROOF_FILE, "rb");
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
      /* Run matching key size and all InvalidKeySize groups. */
      in_group = (key_size_bits == want_bits ||
                  (key_size_bits != 128 && key_size_bits != 192 &&
                   key_size_bits != 256));
      continue;
    }

    if (!in_group)
      continue;

    if (strncmp(p, "\"key\":", 6) == 0 || strncmp(p, "\"msg\":", 6) == 0 ||
        strncmp(p, "\"tag\":", 6) == 0 || strncmp(p, "\"result\":", 9) == 0)
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
      else if (strncmp(p, "\"msg\":", 6) == 0) { dest = msg_hex; dest_sz = sizeof(msg_hex); }
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
        uint8_t key[40];
        uint8_t msg[64];
        uint8_t tag[AES_CMAC_TAG_MAX];
        uint8_t out[AES_CMAC_TAG_MAX];
        size_t key_len, msg_len, tag_len;
        int expect_ok = (strcmp(result, "valid") == 0);

        key_len = cmac_decode_hex(key_hex, key, sizeof(key));
        msg_len = cmac_decode_hex(msg_hex, msg, sizeof(msg));
        tag_len = cmac_decode_hex(tag_hex, tag, sizeof(tag));

        if (key_len == SIZE_MAX || msg_len == SIZE_MAX ||
            (tag_len == SIZE_MAX && expect_ok))
        {
          ++failed;
          have = 0;
          continue;
        }

        ++ran;
        if (expect_ok)
          ++ran_valid;
        else
          ++ran_invalid;

        if (key_len != AES_KEYLEN)
        {
          /* Fixed key-size API cannot accept this key; must be invalid. */
          if (expect_ok)
            ++failed;
        }
        else if (tag_len != AES_CMAC_TAG_MAX)
        {
          ++failed;
        }
        else if (expect_ok)
        {
          if (AES_CMAC(key, msg_len ? msg : NULL, msg_len, out, tag_len) != AES_OK ||
              memcmp(out, tag, tag_len) != 0 ||
              AES_CMAC_verify(key, msg_len ? msg : NULL, msg_len, tag, tag_len) != AES_OK)
            ++failed;
        }
        else
        {
          if (AES_CMAC_verify(key, msg_len ? msg : NULL, msg_len, tag, tag_len) != AES_ERR)
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

/* CAVP CMAC max message is 65536 bytes (hex line ~131 KiB). Host-test BSS only. */
#define CMAC_CAVP_MSG_MAX 65536u
#define CMAC_CAVP_LINE_MAX (CMAC_CAVP_MSG_MAX * 2u + 64u)

/* Parse one full CAVP .rsp for the active AES_KEYLEN. Mlen=0 → empty message. */
static MunitResult cmac_run_cavp_file(const char* path, int is_verify,
                                      unsigned* ran_out)
{
  FILE* file;
  static char line[CMAC_CAVP_LINE_MAX];
  static uint8_t msg[CMAC_CAVP_MSG_MAX];
  uint8_t key[32];
  uint8_t mac[AES_CMAC_TAG_MAX];
  uint8_t out[AES_CMAC_TAG_MAX];
  size_t key_len = 0;
  size_t msg_len = 0;
  size_t mac_len = 0;
  size_t mlen = 0;
  size_t tlen = 0;
  int have_key = 0, have_msg = 0, have_mac = 0, have_result = 0;
  int result_pass = 0;
  unsigned ran = 0;
  unsigned failed = 0;

  file = fopen(path, "rb");
  if (file == NULL)
    return MUNIT_ERROR;

  while (fgets(line, (int)sizeof(line), file) != NULL)
  {
    char* p = line;
    while (*p == ' ' || *p == '\t')
      ++p;
    if (*p == '#' || *p == '\0' || *p == '\n' || *p == '\r')
      continue;

    if (strncmp(p, "Count", 5) == 0)
    {
      have_key = have_msg = have_mac = have_result = 0;
      key_len = msg_len = mac_len = mlen = tlen = 0;
      continue;
    }
    if (strncmp(p, "Mlen", 4) == 0)
    {
      char* eq = strchr(p, '=');
      if (eq != NULL)
        mlen = (size_t)strtoul(eq + 1, NULL, 10);
      continue;
    }
    if (strncmp(p, "Tlen", 4) == 0)
    {
      char* eq = strchr(p, '=');
      if (eq != NULL)
        tlen = (size_t)strtoul(eq + 1, NULL, 10);
      continue;
    }
    if (strncmp(p, "Key", 3) == 0)
    {
      char* eq = strchr(p, '=');
      if (eq != NULL)
      {
        key_len = cmac_decode_hex(eq + 1, key, sizeof(key));
        have_key = (key_len != SIZE_MAX);
      }
      continue;
    }
    if (strncmp(p, "Msg", 3) == 0)
    {
      char* eq = strchr(p, '=');
      if (eq != NULL)
      {
        if (mlen == 0)
        {
          msg_len = 0;
          have_msg = 1;
        }
        else if (mlen > CMAC_CAVP_MSG_MAX)
        {
          have_msg = 0;
        }
        else
        {
          msg_len = cmac_decode_hex(eq + 1, msg, CMAC_CAVP_MSG_MAX);
          have_msg = (msg_len != SIZE_MAX && msg_len == mlen);
        }
      }
      continue;
    }
    if (strncmp(p, "Mac", 3) == 0)
    {
      char* eq = strchr(p, '=');
      if (eq != NULL)
      {
        mac_len = cmac_decode_hex(eq + 1, mac, sizeof(mac));
        have_mac = (mac_len != SIZE_MAX && tlen != 0 && mac_len == tlen);
      }
      if (!is_verify && have_key && have_msg && have_mac &&
          key_len == AES_KEYLEN)
      {
        ++ran;
        if (AES_CMAC(key, msg_len ? msg : NULL, msg_len, out, tlen) != AES_OK ||
            memcmp(out, mac, tlen) != 0)
          ++failed;
        have_key = have_msg = have_mac = 0;
      }
      continue;
    }
    if (strncmp(p, "Result", 6) == 0)
    {
      char* eq = strchr(p, '=');
      result_pass = 0;
      if (eq != NULL)
      {
        while (*eq == '=' || *eq == ' ')
          ++eq;
        result_pass = (*eq == 'P' || *eq == 'p');
      }
      have_result = 1;
      if (is_verify && have_key && have_msg && have_mac && have_result &&
          key_len == AES_KEYLEN)
      {
        int vr;
        ++ran;
        vr = AES_CMAC_verify(key, msg_len ? msg : NULL, msg_len, mac, tlen);
        if (result_pass)
        {
          if (vr != AES_OK)
            ++failed;
        }
        else if (vr != AES_ERR)
          ++failed;
        have_key = have_msg = have_mac = have_result = 0;
      }
      continue;
    }
  }

  fclose(file);
  if (ran_out != NULL)
    *ran_out = ran;
  munit_assert_uint(failed, ==, 0);
  munit_assert_uint(ran, >, 0);
  return MUNIT_OK;
}

static MunitResult test_cmac_cavp_gen(const MunitParameter params[], void* data)
{
  char path[256];
  unsigned ran = 0;
  MunitResult r;

  (void) params;
  (void) data;

#if !defined(AES192) && !defined(AES256)
  snprintf(path, sizeof(path), "%s/CMACGenAES128.rsp", CMAC_CAVP_DIR);
#elif defined(AES192) && (AES192 == 1)
  snprintf(path, sizeof(path), "%s/CMACGenAES192.rsp", CMAC_CAVP_DIR);
#else
  snprintf(path, sizeof(path), "%s/CMACGenAES256.rsp", CMAC_CAVP_DIR);
#endif
  r = cmac_run_cavp_file(path, 0, &ran);
  if (r != MUNIT_OK)
    return r;
  /* Full NIST CAVS 11.0 CMAC Gen AES counts. */
#if !defined(AES192) && !defined(AES256)
  munit_assert_uint(ran, ==, 96u);
#elif defined(AES192) && (AES192 == 1)
  munit_assert_uint(ran, ==, 144u);
#else
  munit_assert_uint(ran, ==, 96u);
#endif
  return MUNIT_OK;
}

static MunitResult test_cmac_cavp_ver(const MunitParameter params[], void* data)
{
  char path[256];
  unsigned ran = 0;
  MunitResult r;

  (void) params;
  (void) data;

#if !defined(AES192) && !defined(AES256)
  snprintf(path, sizeof(path), "%s/CMACVerAES128.rsp", CMAC_CAVP_DIR);
#elif defined(AES192) && (AES192 == 1)
  snprintf(path, sizeof(path), "%s/CMACVerAES192.rsp", CMAC_CAVP_DIR);
#else
  snprintf(path, sizeof(path), "%s/CMACVerAES256.rsp", CMAC_CAVP_DIR);
#endif
  r = cmac_run_cavp_file(path, 1, &ran);
  if (r != MUNIT_OK)
    return r;
  /* Full NIST CAVS 11.0 CMAC Ver AES counts. */
#if !defined(AES192) && !defined(AES256)
  munit_assert_uint(ran, ==, 240u);
#elif defined(AES192) && (AES192 == 1)
  munit_assert_uint(ran, ==, 360u);
#else
  munit_assert_uint(ran, ==, 240u);
#endif
  return MUNIT_OK;
}

MunitResult test_cmac(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;

  cmac_initialize_sbox();

  if (test_cmac_sp800_38b(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_cmac_api(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_cmac_wycheproof(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_cmac_cavp_gen(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  if (test_cmac_cavp_ver(params, data) != MUNIT_OK)
    return MUNIT_FAIL;
  return MUNIT_OK;
}

#else /* !CMAC */

MunitResult test_cmac(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;
  return MUNIT_SKIP;
}

#endif
