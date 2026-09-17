/*
 * SPDX-License-Identifier: Unlicense
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 *
 * EAX RFC and Wycheproof tests. This translation unit is test-only.
 */

#include "aes.h"
#include "munit.h"

#include <stdio.h>
#include <string.h>

#ifndef EAX_VECTOR_FILE
#define EAX_VECTOR_FILE "test_vectors/eax/aes_eax_test.json"
#endif

#if (defined(AES_ENABLE_EAX) && (AES_ENABLE_EAX == 1)) || \
    (defined(AES_ENABLE_EAX_PRIME) && (AES_ENABLE_EAX_PRIME == 1))

#if defined(AES_ENABLE_EAX) && (AES_ENABLE_EAX == 1)

#if !defined(AES192) && !defined(AES256)
struct eax_rfc_vector
{
  const char* key;
  const char* nonce;
  const char* aad;
  const char* msg;
  const char* cipher_and_tag;
};

static const struct eax_rfc_vector eax_rfc_vectors[] = {
  { "233952DEE4D5ED5F9B9C6D6FF80FF478",
    "62EC67F9C3A4A407FCB2A8C49031A8B3", "6BFB914FD07EAE6B", "",
    "E037830E8389F27B025A2D6527E79D01" },
  { "91945D3F4DCBEE0BF45EF52255F095A4",
    "BECAF043B0A23D843194BA972C66DEBD", "FA3BFD4806EB53FA", "F7FB",
    "19DD5C4C9331049D0BDAB0277408F67967E5" },
  { "01F74AD64077F2E704C0F60ADA3DD523",
    "70C3DB4F0D26368400A10ED05D2BFF5E", "234A3463C1264AC6", "1A47CB4933",
    "D851D5BAE03A59F238A23E39199DC9266626C40F80" },
  { "D07CF6CBB7F313BDDE66B727AFD3C5E8",
    "8408DFFF3C1A2B1292DC199E46B7D617", "33CCE2EABFF5A79D", "481C9E39B1",
    "632A9D131AD4C168A4225D8E1FF755939974A7BEDE" },
  { "35B6D0580005BBC12B0587124557D2C2",
    "FDB6B06676EEDC5C61D74276E1F8E816", "AEB96EAEBE2970E9", "40D0C07DA5E4",
    "071DFE16C675CB0677E536F73AFE6A14B74EE49844DD" },
  { "BD8E6E11475E60B268784C38C62FEB22",
    "6EAC5C93072D8E8513F750935E46DA1B", "D4482D1CA78DCE0F",
    "4DE3B35C3FC039245BD1FB7D",
    "835BB4F15D743E350E728414ABB8644FD6CCB86947C5E10590210A4F" },
  { "7C77D6E813BED5AC98BAA417477A2E7D",
    "1A8C98DCD73D38393B2BF1569DEEFC19", "65D2017990D62528",
    "8B0A79306C9CE7ED99DAE4F87F8DD61636",
    "02083E3979DA014812F59F11D52630DA30137327D10649B0AA6E1C181DB617D7F2" },
  { "5FFF20CAFAB119CA2FC73549E20F5B0D",
    "DDE59B97D722156D4D9AFF2BC7559826", "54B9F04E6A09189A",
    "1BDA122BCE8A8DBAF1877D962B8592DD2D56",
    "2EC47B2C4954A489AFC7BA4897EDCDAE8CC33B60450599BD02C96382902AEF7F832A" },
  { "A4A4782BCFFD3EC5E7EF6D8C34A56123",
    "B781FCF2F75FA5A8DE97A9CA48E522EC", "899A175897561D7E",
    "6CF36720872B8513F6EAB1A8A44438D5EF11",
    "0DE18FD0FDD91E7AF19F1D8EE8733938B1E8E7F6D2231618102FDB7FE55FF1991700" },
  { "8395FCF1E95BEBD697BD010BC766AAC3",
    "22E7ADD93CFC6393C57EC0B3C17D6B44", "126735FCC320D25A",
    "CA40D7446E545FFAED3BD12A740A659FFBBB3CEAB7",
    "CB8920F87A6C75CFF39627B56E3ED197C552D295A7CFC46AFC253B4652B1AF3795B124AB6E" }
};
#endif

static int eax_hex_value(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static size_t eax_decode_hex(const char* text, uint8_t* output,
                             size_t capacity)
{
  size_t length = 0;
  while (*text != '\0')
  {
    const int high = eax_hex_value((unsigned char)*text++);
    int low;
    if (high < 0) continue;
    low = eax_hex_value((unsigned char)*text++);
    if (low < 0 || length == capacity) return SIZE_MAX;
    output[length++] = (uint8_t)((high << 4) | low);
  }
  return length;
}

#if !defined(AES192) && !defined(AES256)
static int eax_decode_vector(const struct eax_rfc_vector* vector,
                             uint8_t* key, uint8_t* nonce, uint8_t* aad,
                             uint8_t* msg, uint8_t* cipher, uint8_t* tag,
                             size_t* key_len, size_t* nonce_len,
                             size_t* aad_len, size_t* msg_len,
                             size_t* cipher_len, size_t* tag_len)
{
  *key_len = eax_decode_hex(vector->key, key, 32);
  *nonce_len = eax_decode_hex(vector->nonce, nonce, 2048);
  *aad_len = eax_decode_hex(vector->aad, aad, 2048);
  *msg_len = eax_decode_hex(vector->msg, msg, 2048);
  *cipher_len = eax_decode_hex(vector->cipher_and_tag, cipher, 2048);
  if (*key_len == SIZE_MAX || *nonce_len == SIZE_MAX || *aad_len == SIZE_MAX ||
      *msg_len == SIZE_MAX || *cipher_len == SIZE_MAX || *cipher_len < 16)
    return 0;
  *tag_len = 16;
  memcpy(tag, cipher + *cipher_len - *tag_len, *tag_len);
  *cipher_len -= *tag_len;
  return 1;
}
#endif

static MunitResult test_eax_rfc(const MunitParameter params[], void* data)
{
  size_t i;

  (void) params;
  (void) data;
#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
  AES_init_sbox();
#endif
#if !defined(AES192) && !defined(AES256)
  for (i = 0; i < sizeof(eax_rfc_vectors) / sizeof(eax_rfc_vectors[0]); ++i)
  {
    uint8_t key[32], nonce[2048], aad[2048], msg[2048];
    uint8_t expected[2048], tag[16], generated[16], output[2048];
    size_t key_len, nonce_len, aad_len, msg_len, cipher_len, tag_len;

    munit_assert_true(eax_decode_vector(&eax_rfc_vectors[i], key, nonce, aad,
                                        msg, expected, tag, &key_len,
                                        &nonce_len, &aad_len, &msg_len,
                                        &cipher_len, &tag_len));
    munit_assert_int(AES_EAX_encrypt(key, nonce, nonce_len, aad, aad_len, msg,
                                     msg_len, output, generated, tag_len), ==,
                     AES_OK);
    munit_assert_memory_equal(cipher_len, output, expected);
    munit_assert_memory_equal(tag_len, generated, tag);
    munit_assert_int(AES_EAX_decrypt(key, nonce, nonce_len, aad, aad_len,
                                     expected, cipher_len, tag, tag_len,
                                     output), ==, AES_OK);
    munit_assert_memory_equal(msg_len, output, msg);
  }
#else
  (void)i;
#endif
  return MUNIT_OK;
}

static int eax_json_field(const char* line, const char* name, char* output,
                          size_t capacity)
{
  char marker[32];
  const char* start;
  const char* end;
  size_t length;

  (void) snprintf(marker, sizeof(marker), "\"%s\"", name);
  start = strstr(line, marker);
  if (start == NULL) return 0;
  start = strchr(start + strlen(marker), ':');
  if (start == NULL || (start = strchr(start + 1, '"')) == NULL) return 0;
  end = strchr(start + 1, '"');
  if (end == NULL) return 0;
  length = (size_t)(end - start - 1);
  if (length >= capacity) return 0;
  memcpy(output, start + 1, length);
  output[length] = '\0';
  return 1;
}

static MunitResult test_eax_wycheproof(const MunitParameter params[], void* data)
{
  FILE* file;
  char line[4096];
  char key_text[4096] = { 0 }, iv_text[4096] = { 0 };
  char aad_text[4096] = { 0 }, msg_text[4096] = { 0 };
  char ct_text[4096] = { 0 }, tag_text[4096] = { 0 };
  char result[32] = { 0 };
  unsigned tc_id = 0;
  size_t vector_count = 0;

  (void) params;
  (void) data;
  file = fopen(EAX_VECTOR_FILE, "rb");
  munit_assert_not_null(file);
  while (fgets(line, sizeof(line), file) != NULL)
  {
    unsigned parsed_id;
    if (sscanf(line, " %*[^0-9]%u", &parsed_id) == 1 &&
        strstr(line, "\"tcId\"") != NULL)
    {
      tc_id = parsed_id;
      key_text[0] = iv_text[0] = aad_text[0] = msg_text[0] = '\0';
      ct_text[0] = tag_text[0] = result[0] = '\0';
    }
    (void) eax_json_field(line, "key", key_text, sizeof(key_text));
    (void) eax_json_field(line, "iv", iv_text, sizeof(iv_text));
    (void) eax_json_field(line, "aad", aad_text, sizeof(aad_text));
    (void) eax_json_field(line, "msg", msg_text, sizeof(msg_text));
    (void) eax_json_field(line, "ct", ct_text, sizeof(ct_text));
    (void) eax_json_field(line, "tag", tag_text, sizeof(tag_text));
    if (eax_json_field(line, "result", result, sizeof(result)))
    {
      uint8_t key[32], iv[2048], aad[2048], msg[2048], ct[2048], tag[32];
      uint8_t output[2048], generated[32];
      const size_t key_len = eax_decode_hex(key_text, key, sizeof(key));
      const size_t iv_len = eax_decode_hex(iv_text, iv, sizeof(iv));
      const size_t aad_len = eax_decode_hex(aad_text, aad, sizeof(aad));
      const size_t msg_len = eax_decode_hex(msg_text, msg, sizeof(msg));
      const size_t ct_len = eax_decode_hex(ct_text, ct, sizeof(ct));
      const size_t tag_len = eax_decode_hex(tag_text, tag, sizeof(tag));

      munit_assert_uint(tc_id, >, 0);
      ++vector_count;
      if (key_len != AES_KEYLEN)
        continue;
      munit_assert_size(iv_len, !=, SIZE_MAX);
      munit_assert_size(aad_len, !=, SIZE_MAX);
      munit_assert_size(msg_len, !=, SIZE_MAX);
      munit_assert_size(ct_len, !=, SIZE_MAX);
      munit_assert_size(tag_len, ==, 16);
      if (strcmp(result, "valid") == 0)
      {
        munit_assert_int(AES_EAX_encrypt(key, iv, iv_len, aad, aad_len, msg,
                                         msg_len, output, generated, tag_len),
                         ==, AES_OK);
        munit_assert_memory_equal(ct_len, output, ct);
        munit_assert_memory_equal(tag_len, generated, tag);
      }
      memset(output, 0xa5, sizeof(output));
      munit_assert_int(AES_EAX_decrypt(key, iv, iv_len, aad, aad_len, ct,
                                       ct_len, tag, tag_len, output), ==,
                       strcmp(result, "valid") == 0 ? AES_OK :
                                                       AES_ERR);
      if (strcmp(result, "valid") == 0)
        munit_assert_memory_equal(msg_len, output, msg);
    }
  }
  fclose(file);
  munit_assert_size(vector_count, ==, 240);
  return MUNIT_OK;
}

static MunitResult test_eax_api(const MunitParameter params[], void* data)
{
  static const uint8_t key[16] = {
    0x23, 0x39, 0x52, 0xde, 0xe4, 0xd5, 0xed, 0x5f,
    0x9b, 0x9c, 0x6d, 0x6f, 0xf8, 0x0f, 0xf4, 0x78
  };
  static const uint8_t nonce[16] = {
    0x62, 0xec, 0x67, 0xf9, 0xc3, 0xa4, 0xa4, 0x07,
    0xfc, 0xb2, 0xa8, 0xc4, 0x90, 0x31, 0xa8, 0xb3
  };
  static const uint8_t aad[8] = {
    0x6b, 0xfb, 0x91, 0x4f, 0xd0, 0x7e, 0xae, 0x6b
  };
  uint8_t message[32] = { 0 };
  uint8_t ciphertext[32] = { 0 };
  uint8_t tag[16] = { 0 };
  uint8_t bad[32];
  uint8_t bad_tag[16];
  uint8_t empty_tag[1];
  uint8_t untouched[32];

  (void) params;
  (void) data;
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), message, sizeof(message),
                                   ciphertext, tag, sizeof(tag)), ==,
                   AES_OK);

  memcpy(bad, message, sizeof(message));
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), bad, sizeof(bad), bad, tag,
                                   sizeof(tag)), ==, AES_OK);
  munit_assert_memory_equal(sizeof(ciphertext), bad, ciphertext);

  memcpy(bad, ciphertext, sizeof(ciphertext));
  munit_assert_int(AES_EAX_decrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), bad, sizeof(bad), tag,
                                   sizeof(tag), bad), ==, AES_OK);
  munit_assert_memory_equal(sizeof(message), bad, message);

  memcpy(bad_tag, tag, sizeof(bad_tag));
  bad_tag[0] ^= 1;
  memset(bad, 0xa5, sizeof(bad));
  memset(untouched, 0xa5, sizeof(untouched));
  munit_assert_int(AES_EAX_decrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), ciphertext, sizeof(ciphertext),
                   bad_tag, sizeof(bad_tag), bad), ==,
                   AES_ERR);
  munit_assert_memory_equal(sizeof(bad), bad, untouched);

  bad[0] = ciphertext[0] ^ 1;
  memcpy(bad + 1, ciphertext + 1, sizeof(ciphertext) - 1);
  munit_assert_int(AES_EAX_decrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), bad, sizeof(bad), tag,
                                   sizeof(tag), NULL), ==, AES_ERR);
  munit_assert_int(AES_EAX_decrypt(key, nonce, sizeof(nonce), aad,
                                   sizeof(aad), ciphertext, sizeof(ciphertext),
                                   tag, sizeof(tag), NULL), ==, AES_ERR);

  bad[0] = aad[0] ^ 1;
  munit_assert_int(AES_EAX_decrypt(key, nonce, sizeof(nonce), bad,
                                   sizeof(aad), ciphertext, sizeof(ciphertext),
                                   tag, sizeof(tag), bad), ==, AES_ERR);
  bad[0] = nonce[0] ^ 1;
  munit_assert_int(AES_EAX_decrypt(key, bad, sizeof(nonce), aad, sizeof(aad),
                                   ciphertext, sizeof(ciphertext), tag,
                                   sizeof(tag), bad), ==, AES_ERR);

  munit_assert_int(AES_EAX_encrypt(key, NULL, 0, NULL, 0, NULL, 0, NULL,
                                   tag, sizeof(tag)), ==, AES_OK);
  munit_assert_int(AES_EAX_decrypt(key, NULL, 0, NULL, 0, NULL, 0, tag,
                                   sizeof(tag), NULL), ==, AES_OK);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext, tag,
                                   sizeof(tag) + 1), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(NULL, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext, tag,
                                   sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, NULL, 1, aad, sizeof(aad), message,
                                   sizeof(message), ciphertext, tag,
                                   sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), NULL, 1, message,
                                   sizeof(message), ciphertext, tag,
                                   sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext, NULL,
                                   sizeof(tag)), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext,
                                   empty_tag, 0), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext, tag,
                                   7), ==, AES_ERR);
  munit_assert_int(AES_EAX_encrypt(key, nonce, sizeof(nonce), aad, sizeof(aad),
                                   message, sizeof(message), ciphertext, tag,
                                   8), ==, AES_OK);
  return MUNIT_OK;
}

MunitResult test_eax(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;
  return test_eax_rfc(params, data) == MUNIT_OK &&
                 test_eax_wycheproof(params, data) == MUNIT_OK &&
                 test_eax_api(params, data) == MUNIT_OK ?
             MUNIT_OK : MUNIT_FAIL;
}

#endif /* EAX */

#if defined(AES_ENABLE_EAX_PRIME) && (AES_ENABLE_EAX_PRIME == 1)

static MunitResult test_eax_prime_worked(const MunitParameter params[],
                                         void* data)
{
  static const uint8_t keys[][16] = {
    { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 },
    { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f },
    { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
      0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f }
    ,{ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
      0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f }
    ,{ 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
      0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f }
    ,{ 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
      0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f }
    ,{ 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
      0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f }
    ,{ 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
      0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f }
    ,{ 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f }
    ,{ 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
      0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f }
    ,{ 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf }
    ,{ 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
      0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf }
  };
  static const uint8_t cleartext[][80] = {
    { 0 },
    { 0x10 },
    { 0x20, 0x45, 0x6a, 0x8f, 0xb4, 0xd9, 0xfe, 0x23,
      0x48, 0x6d, 0x92, 0xb7, 0xdc, 0x01, 0x26 }
    ,{ 0x30, 0x55, 0x7a, 0x9f, 0xc4, 0xe9, 0x0e, 0x33,
      0x58, 0x7d, 0xa2, 0xc7, 0xec, 0x11, 0x36, 0x5b }
    ,{ 0x40, 0x65, 0x8a, 0xaf, 0xd4, 0xf9, 0x1e, 0x43,
      0x68, 0x8d, 0xb2, 0xd7, 0xfc, 0x21, 0x46, 0x6b, 0x90 }
    ,{ 0x50, 0x75, 0x9a, 0xbf, 0xe4, 0x09, 0x2e, 0x53,
      0x78, 0x9d, 0xc2, 0xe7, 0x0c, 0x31, 0x56, 0x7b, 0xa0,
      0xc5, 0xea, 0x0f, 0x34, 0x59, 0x7e, 0xa3, 0xc8,
      0xed, 0x12, 0x37, 0x5c, 0x81, 0xa6 }
    ,{ 0x60, 0x85, 0xaa, 0xcf, 0xf4, 0x19, 0x3e, 0x63,
      0x88, 0xad, 0xd2, 0xf7, 0x1c, 0x41, 0x66, 0x8b,
      0xb0, 0xd5, 0xfa, 0x1f, 0x44, 0x69, 0x8e, 0xb3,
      0xd8, 0xfd, 0x22, 0x47, 0x6c, 0x91, 0xb6, 0xdb }
    ,{ 0x70, 0x95, 0xba, 0xdf, 0x04, 0x29, 0x4e, 0x73,
      0x98, 0xbd, 0xe2, 0x07, 0x2c, 0x51, 0x76, 0x9b,
      0xc0, 0xe5, 0x0a, 0x2f, 0x54, 0x79, 0x9e, 0xc3,
      0xe8, 0x0d, 0x32, 0x57, 0x7c, 0xa1, 0xc6, 0xeb, 0x10 }
    ,{ 0x80, 0xa5, 0xca, 0xef, 0x14, 0x39, 0x5e, 0x83,
      0xa8, 0xcd, 0xf2, 0x17, 0x3c, 0x61, 0x86, 0xab,
      0xd0, 0xf5, 0x1a, 0x3f, 0x64, 0x89, 0xae, 0xd3,
      0xf8, 0x1d, 0x42, 0x67, 0x8c, 0xb1, 0xd6, 0xfb,
      0x20, 0x45, 0x6a, 0x8f, 0xb4, 0xd9, 0xfe, 0x23,
      0x48, 0x6d, 0x92, 0xb7, 0xdc, 0x01, 0x26 }
    ,{ 0x90 }
    ,{ 0xa0, 0xc5, 0xea, 0x0f, 0x34, 0x59, 0x7e, 0xa3,
      0xc8, 0xed, 0x12, 0x37, 0x5c, 0x81, 0xa6, 0xcb,
      0xf0, 0x15, 0x3a, 0x5f, 0x84, 0xa9, 0xce, 0xf3,
      0x18, 0x3d, 0x62, 0x87, 0xac, 0xd1, 0xf6, 0x1b,
      0x40, 0x65, 0x8a, 0xaf, 0xd4, 0xf9, 0x1e, 0x43,
      0x68, 0x8d, 0xb2, 0xd7, 0xfc, 0x21, 0x46, 0x6b, 0x90,
      0xb5, 0xda, 0xff, 0x24, 0x49, 0x6e, 0x93, 0xb8, 0xdd, 0x02, 0x27, 0x4c, 0x71, 0x96, 0xbb }
    ,{ 0xb0, 0xd5, 0xfa, 0x1f, 0x44, 0x69, 0x8e, 0xb3,
      0xd8, 0xfd, 0x22, 0x47, 0x6c, 0x91, 0xb6, 0xdb,
      0x00, 0x25, 0x4a, 0x6f, 0x94, 0xb9, 0xde, 0x03,
      0x28, 0x4d, 0x72, 0x97, 0xbc, 0xe1, 0x06, 0x2b,
      0x50, 0x75, 0x9a, 0xbf, 0xe4, 0x09, 0x2e, 0x53,
      0x78, 0x9d, 0xc2, 0xe7, 0x0c, 0x31, 0x56, 0x7b,
      0xa0, 0xc5, 0xea, 0x0f, 0x34, 0x59, 0x7e, 0xa3,
      0xc8, 0xed, 0x12, 0x37, 0x5c, 0x81, 0xa6, 0xcb,
      0xf0, 0x15, 0x3a, 0x5f, 0x84, 0xa9, 0xce, 0xf3,
      0x18, 0x3d, 0x62, 0x87, 0xac, 0xd1, 0xf6 }
  };
  static const size_t cleartext_len[] = { 0, 1, 15, 16, 17, 31, 32, 33,
                                          47, 1, 64, 79 };
  static const uint8_t plaintext[][80] = {
    { 0 },
    { 0xef },
    { 0xdf, 0xcc, 0xf9, 0xe6, 0x93, 0x80, 0xad, 0x5a,
      0x47, 0x74, 0x61, 0x0e, 0x3b, 0x28, 0xd5, 0xc2 }
    ,{ 0xcf, 0xdc, 0xe9, 0xf6, 0x83, 0x90, 0xbd, 0x4a,
      0x57, 0x64, 0x71, 0x1e, 0x2b, 0x38, 0xc5 }
    ,{ 0xbf, 0xac, 0x99, 0x86, 0xf3, 0xe0, 0xcd, 0x3a,
      0x27, 0x14, 0x01, 0x6e, 0x5b, 0x48, 0xb5, 0xa2, 0x8f }
    ,{ 0xaf, 0xbc, 0x89, 0x96, 0xe3, 0xf0, 0xdd, 0x2a,
      0x37, 0x04, 0x11, 0x7e, 0x4b, 0x58, 0xa5, 0xb2, 0x9f,
      0xec, 0xf9, 0xc6, 0xd3, 0x20, 0x0d, 0x1a, 0x67, 0x74, 0x41, 0xae, 0xbb, 0x88, 0x95, 0xe2 }
    ,{ 0x9f, 0x8c, 0xb9, 0xa6, 0xd3, 0xc0, 0xed, 0x1a,
      0x07, 0x34, 0x21, 0x4e, 0x7b, 0x68, 0x95, 0x82,
      0xaf, 0xdc, 0xc9, 0xf6, 0xe3, 0x10, 0x3d, 0x2a, 0x57, 0x44, 0x71, 0x9e, 0x8b, 0xb8, 0xa5 }
    ,{ 0x8f, 0x9c, 0xa9, 0xb6, 0xc3, 0xd0, 0xfd, 0x0a,
      0x17, 0x24, 0x31, 0x5e, 0x6b, 0x78, 0x85, 0x92,
      0xbf, 0xcc, 0xd9, 0xe6, 0xf3, 0x00, 0x2d, 0x3a, 0x47, 0x54, 0x61, 0x8e, 0x9b, 0xa8, 0xb5, 0xc2, 0xef }
    ,{ 0x7f }
    ,{ 0x6f, 0x7c, 0x49, 0x56, 0x23, 0x30, 0x1d, 0xea,
      0xf7, 0xc4, 0xd1, 0xbe, 0x8b, 0x98, 0x65, 0x72,
      0x5f, 0x2c, 0x39, 0x06, 0x13, 0xe0, 0xcd, 0xda, 0xa7, 0xb4, 0x81, 0x6e, 0x7b, 0x48, 0x55, 0x22, 0x0f, 0x1c, 0xe9, 0xf6, 0xc3, 0xd0, 0xbd, 0x8a, 0x97, 0x64, 0x71, 0x5e, 0x2b, 0x38, 0x05 }
    ,{ 0x5f, 0x4c, 0x79, 0x66, 0x13, 0x00, 0x2d, 0xda,
      0xc7, 0xf4, 0xe1, 0x8e, 0xbb, 0xa8, 0x55, 0x42,
      0x6f, 0x1c, 0x09, 0x36, 0x23, 0xd0, 0xfd, 0xea, 0x97, 0x84, 0xb1, 0x5e, 0x4b, 0x78, 0x65, 0x12, 0x3f, 0x2c, 0xd9, 0xc6, 0xf3, 0xe0, 0x8d, 0xba, 0xa7, 0x54, 0x41, 0x6e, 0x1b, 0x08, 0x35, 0x22, 0xcf, 0xfc, 0xe9, 0x96, 0x83, 0xb0, 0x5d, 0x4a, 0x77, 0x64, 0x11, 0x3e, 0x2b, 0xd8, 0xc5, 0xf2 }
    ,{ 0x4f, 0x5c, 0x69, 0x76, 0x03, 0x10, 0x3d, 0xca,
      0xd7, 0xe4, 0xf1, 0x9e, 0xab, 0xb8, 0x45, 0x52,
      0x7f, 0x0c, 0x19, 0x26, 0x33, 0xc0, 0xed, 0xfa, 0x87, 0x94, 0xa1, 0x4e, 0x5b, 0x68, 0x75, 0x02, 0x2f, 0x3c, 0xc9, 0xd6, 0xe3, 0xf0, 0x9d, 0xaa, 0xb7, 0x44, 0x51, 0x7e, 0x0b, 0x18, 0x25, 0x32, 0xdf, 0xec, 0xf9, 0x86, 0x93, 0xa0, 0x4d, 0x5a, 0x67, 0x74, 0x01, 0x2e, 0x3b, 0xc8, 0xd5, 0xe2, 0x8f }
  };
  static const size_t plaintext_len[] = { 0, 1, 16, 15, 17, 32, 31, 33,
                                          1, 47, 64, 65 };
  static const uint8_t expected_ciphertext[][80] = {
    { 0 },
    { 0x88 },
    { 0x16, 0x9b, 0x3f, 0xea, 0xd4, 0x21, 0xfa, 0x34,
      0xc4, 0xd6, 0x24, 0xe2, 0xda, 0xcf, 0x68, 0x49 }
    ,{ 0xe3, 0x70, 0xa5, 0xee, 0x2f, 0xe7, 0xfe, 0x36,
      0x18, 0x4d, 0x81, 0xe4, 0x99, 0xb1, 0xd7 }
    ,{ 0x2e, 0x34, 0x24, 0x7e, 0x12, 0x32, 0x42, 0x09,
      0x97, 0xa2, 0xa2, 0x39, 0x0f, 0xd9, 0x80, 0x88, 0x70 }
    ,{ 0xf5, 0x85, 0x9e, 0x8d, 0x22, 0x6f, 0x75, 0xe4,
      0x5d, 0xbd, 0xd7, 0x42, 0x42, 0x0b, 0x35, 0x8e,
      0x6a, 0xa4, 0xf4, 0x52, 0xe7, 0x75, 0x9b, 0x90,
      0xe1, 0x28, 0x3f, 0x4f, 0xec, 0x69, 0x0d, 0x6b }
    ,{ 0x6b, 0x98, 0xe8, 0x56, 0xcb, 0x5e, 0xbc, 0xb3,
      0x3d, 0xf5, 0x55, 0xdd, 0xd3, 0xfe, 0x5a, 0x17,
      0x73, 0x93, 0xa6, 0x8a, 0x45, 0xc2, 0x07, 0x83,
      0x58, 0x57, 0xbc, 0x67, 0x34, 0x69, 0x05 }
    ,{ 0x27, 0x13, 0xe8, 0xa5, 0x2d, 0xc7, 0x19, 0x92,
      0x97, 0x3d, 0x16, 0x2a, 0xb4, 0xf4, 0x04, 0xf7,
      0x14, 0x08, 0x56, 0x34, 0xc0, 0xfa, 0x94, 0xa5,
      0x0b, 0xc6, 0x40, 0xc0, 0xa3, 0x7b, 0xf8, 0x33, 0x22 }
    ,{ 0x8e }
    ,{ 0x01, 0x9e, 0x73, 0x71, 0xee, 0x16, 0xea, 0x07,
      0xc0, 0x68, 0xdd, 0xf8, 0x8e, 0x51, 0xf3, 0x2c,
      0x97, 0x8e, 0x5e, 0xc5, 0xfc, 0x4d, 0x30, 0x19,
      0xe7, 0xdd, 0xba, 0x0f, 0x22, 0xdb, 0x84, 0x08,
      0x65, 0x68, 0x15, 0xdd, 0xa8, 0x6c, 0xb2, 0xac,
      0x93, 0xd4, 0x17, 0xd2, 0xcd, 0x32, 0xcf }
    ,{ 0x96, 0x8d, 0xa0, 0xee, 0xcf, 0x9d, 0xef, 0x0f,
      0x17, 0xe2, 0x86, 0x81, 0x3c, 0xae, 0xff, 0xf1,
      0xed, 0xa0, 0x08, 0x21, 0xd3, 0x26, 0x77, 0x10,
      0x29, 0x3d, 0x47, 0x71, 0xf7, 0x66, 0x9c, 0xfc,
      0xac, 0x20, 0xd1, 0xe6, 0x1d, 0xbc, 0x6c, 0xed,
      0x39, 0xea, 0x92, 0xf9, 0x3f, 0xe3, 0x3d, 0xc9,
      0xf4, 0xad, 0x31, 0xe4, 0x9e, 0xa0, 0xc9, 0x2e,
      0x70, 0x0c, 0xb1, 0x53, 0x38, 0x4d, 0x02, 0xf9 }
    ,{ 0x5d, 0x5c, 0xb8, 0x4a, 0x48, 0x76, 0x65, 0x6a,
      0x1d, 0x5c, 0xae, 0xba, 0x75, 0x0a, 0xb2, 0xfd,
      0x29, 0x6a, 0xc0, 0x9a, 0xa5, 0x6f, 0x80, 0x88,
      0xb4, 0x93, 0x91, 0x12, 0x45, 0x80, 0xe0, 0x21,
      0xad, 0xa8, 0xe2, 0x95, 0x4a, 0x07, 0x9b, 0x56,
      0x81, 0x0a, 0x86, 0x12, 0xdf, 0x8a, 0xe1, 0x94,
      0x96, 0x34, 0x7e, 0x28, 0x41, 0x71, 0xf9, 0x91,
      0xb0, 0x17, 0x79, 0x08, 0x78, 0x22, 0x98, 0x82, 0x0b }
  };
  static const uint8_t expected_tag[][4] = {
    { 0xc4, 0x00, 0x75, 0x6f },
    { 0x59, 0x15, 0xdf, 0xb3 },
    { 0x77, 0xa0, 0xb8, 0x4c }, { 0x4e, 0x85, 0x43, 0x21 },
    { 0xb6, 0xe7, 0x95, 0x70 }, { 0x1d, 0xa6, 0xde, 0x94 },
    { 0x69, 0x55, 0xe5, 0x28 }, { 0xdf, 0xd3, 0x05, 0x6f },
    { 0x75, 0xfb, 0x31, 0xba }, { 0xe3, 0x55, 0xcc, 0x81 },
    { 0xcf, 0xa2, 0x15, 0x37 }, { 0xcd, 0x8e, 0x94, 0x68 }
  };
  uint8_t ciphertext[80], tag[4], output[80];
  uint8_t boundary_cleartext[80], boundary_plaintext[80];
  uint8_t boundary_ciphertext[80], boundary_output[80], boundary_tag[4];
  static const size_t boundary_lengths[][2] = {
    { 0, 0 }, { 1, 1 }, { 15, 16 }, { 16, 15 }, { 17, 17 },
    { 31, 32 }, { 32, 31 }, { 33, 33 }, { 47, 1 }, { 1, 47 },
    { 64, 64 }, { 79, 65 }
  };
  size_t i;

  (void) params;
  (void) data;
#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
  AES_init_sbox();
#endif
  for (i = 0; i < sizeof(plaintext_len) / sizeof(plaintext_len[0]); ++i)
  {
    munit_assert_int(AES_EAX_PRIME_encrypt(
                       keys[i], cleartext[i], cleartext_len[i], plaintext[i],
                       plaintext_len[i], ciphertext, tag), ==,
                     AES_OK);
    munit_assert_memory_equal(plaintext_len[i], ciphertext,
                              expected_ciphertext[i]);
    munit_assert_memory_equal(sizeof(tag), tag, expected_tag[i]);
    munit_assert_int(AES_EAX_PRIME_decrypt(
                       keys[i], cleartext[i], cleartext_len[i], ciphertext,
                       plaintext_len[i], tag, output), ==,
                     AES_OK);
    munit_assert_memory_equal(plaintext_len[i], output, plaintext[i]);
  }

  for (i = 0; i < sizeof(boundary_lengths) / sizeof(boundary_lengths[0]); ++i)
  {
    size_t j;
    const size_t cleartext_len = boundary_lengths[i][0];
    const size_t plaintext_len = boundary_lengths[i][1];
    for (j = 0; j < cleartext_len; ++j)
      boundary_cleartext[j] = (uint8_t)(j * 37u + i);
    for (j = 0; j < plaintext_len; ++j)
      boundary_plaintext[j] = (uint8_t)(0xffu - j * 19u - i);
    munit_assert_int(AES_EAX_PRIME_encrypt(
                       keys[0], boundary_cleartext, cleartext_len,
                       boundary_plaintext, plaintext_len, boundary_ciphertext,
                       boundary_tag), ==, AES_OK);
    munit_assert_int(AES_EAX_PRIME_decrypt(
                       keys[0], boundary_cleartext, cleartext_len,
                       boundary_ciphertext, plaintext_len, boundary_tag,
                       boundary_output), ==, AES_OK);
    munit_assert_memory_equal(plaintext_len, boundary_output,
                              boundary_plaintext);
    memcpy(boundary_output, boundary_plaintext, plaintext_len);
    boundary_tag[0] ^= 1;
    munit_assert_int(AES_EAX_PRIME_decrypt(
                       keys[0], boundary_cleartext, cleartext_len,
                       boundary_ciphertext, plaintext_len, boundary_tag,
                       boundary_output), ==, AES_ERR);
    for (j = 0; j < plaintext_len; ++j)
      munit_assert_uint(boundary_output[j], ==, boundary_plaintext[j]);
    boundary_tag[0] ^= 1;

    if (plaintext_len != 0)
    {
      memcpy(boundary_output, boundary_plaintext, plaintext_len);
      boundary_ciphertext[plaintext_len - 1] ^= 1;
      munit_assert_int(AES_EAX_PRIME_decrypt(
                         keys[0], boundary_cleartext, cleartext_len,
                         boundary_ciphertext, plaintext_len, boundary_tag,
                         boundary_output), ==, AES_ERR);
      for (j = 0; j < plaintext_len; ++j)
        munit_assert_uint(boundary_output[j], ==, boundary_plaintext[j]);
      boundary_ciphertext[plaintext_len - 1] ^= 1;
    }
    if (cleartext_len != 0)
    {
      memcpy(boundary_output, boundary_plaintext, plaintext_len);
      boundary_cleartext[cleartext_len - 1] ^= 1;
      munit_assert_int(AES_EAX_PRIME_decrypt(
                         keys[0], boundary_cleartext, cleartext_len,
                         boundary_ciphertext, plaintext_len, boundary_tag,
                         boundary_output), ==, AES_ERR);
      for (j = 0; j < plaintext_len; ++j)
        munit_assert_uint(boundary_output[j], ==, boundary_plaintext[j]);
      boundary_cleartext[cleartext_len - 1] ^= 1;
    }
  }
  return MUNIT_OK;
}

static MunitResult test_eax_prime_c12_22(const MunitParameter params[],
                                         void* data)
{
  static const uint8_t key[16] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
  };
  static const uint8_t cleartext[] = {
    0xa2, 0x0c, 0x06, 0x0a, 0x60, 0x7c, 0x86, 0xf7, 0x54, 0x01,
    0x16, 0x00, 0x7b, 0x02, 0xa7, 0x03, 0x02, 0x01, 0x04, 0xa8,
    0x03, 0x02, 0x01, 0x02, 0xac, 0x0f, 0xa2, 0x0d, 0xa0, 0x0b,
    0xa1, 0x09, 0x80, 0x01, 0x02, 0x81, 0x04, 0x48, 0xf3, 0xd2,
    0xf8, 0xbe, 0x19, 0x28, 0x17, 0x81, 0x15, 0x9a, 0xa6, 0x0d,
    0x06, 0x0b, 0x60, 0x7c, 0x86, 0xf7, 0x54, 0x01, 0x16, 0x00,
    0x7b, 0x82, 0x11, 0x02, 0x48, 0xf3, 0xd2, 0xf8
  };
  static const uint8_t plaintext[16] = {
    0x54, 0x45, 0x4d, 0x50, 0x0b, 0x40, 0x00, 0x07,
    0x00, 0x05, 0x1a, 0x00, 0x00, 0x02, 0x00, 0xe4
  };
  static const uint8_t expected_ciphertext[16] = {
    0x34, 0xb7, 0x27, 0x6f, 0x54, 0x06, 0xd2, 0x5d,
    0x4e, 0x3a, 0x51, 0x73, 0x1d, 0x88, 0xa5, 0xd9
  };
  static const uint8_t expected_tag[4] = { 0x1b, 0xd7, 0x8f, 0x32 };
  uint8_t ciphertext[sizeof(plaintext)];
  uint8_t tag[AES_EAX_PRIME_TAG_LEN];
  uint8_t decrypted[sizeof(plaintext)];
  uint8_t bad_tag[AES_EAX_PRIME_TAG_LEN];
  uint8_t bad_cleartext[sizeof(cleartext)];

  (void) params;
  (void) data;
#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
  AES_init_sbox();
#endif
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, cleartext, sizeof(cleartext), plaintext,
                     sizeof(plaintext), ciphertext, tag), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(expected_ciphertext), ciphertext,
                            expected_ciphertext);
  munit_assert_memory_equal(sizeof(expected_tag), tag, expected_tag);

  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, cleartext, sizeof(cleartext), ciphertext,
                     sizeof(ciphertext), tag, decrypted), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(plaintext), decrypted, plaintext);

  memcpy(decrypted, plaintext, sizeof(plaintext));
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, cleartext, sizeof(cleartext), decrypted,
                     sizeof(decrypted), decrypted, tag), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(expected_ciphertext), decrypted,
                            expected_ciphertext);
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, cleartext, sizeof(cleartext), decrypted,
                     sizeof(decrypted), tag, decrypted), ==,
                   AES_OK);
  munit_assert_memory_equal(sizeof(plaintext), decrypted, plaintext);

  memcpy(bad_tag, tag, sizeof(bad_tag));
  bad_tag[0] ^= 1;
  memset(decrypted, 0xa5, sizeof(decrypted));
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, cleartext, sizeof(cleartext), ciphertext,
                     sizeof(ciphertext), bad_tag, decrypted), ==,
                   AES_ERR);
  for (size_t i = 0; i < sizeof(decrypted); ++i)
    munit_assert_uint(decrypted[i], ==, 0xa5);

  ciphertext[0] ^= 1;
  memset(decrypted, 0xa5, sizeof(decrypted));
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, cleartext, sizeof(cleartext), ciphertext,
                     sizeof(ciphertext), tag, decrypted), ==,
                   AES_ERR);
  for (size_t i = 0; i < sizeof(decrypted); ++i)
    munit_assert_uint(decrypted[i], ==, 0xa5);
  ciphertext[0] ^= 1;

  memcpy(bad_cleartext, cleartext, sizeof(cleartext));
  bad_cleartext[0] ^= 1;
  memset(decrypted, 0xa5, sizeof(decrypted));
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, bad_cleartext, sizeof(bad_cleartext), ciphertext,
                     sizeof(ciphertext), tag, decrypted), ==,
                   AES_ERR);
  for (size_t i = 0; i < sizeof(decrypted); ++i)
    munit_assert_uint(decrypted[i], ==, 0xa5);

  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, cleartext, sizeof(cleartext), plaintext,
                     sizeof(plaintext), ciphertext, NULL), ==,
                   AES_ERR);
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, cleartext, sizeof(cleartext), ciphertext,
                     sizeof(ciphertext), NULL, decrypted), ==,
                   AES_ERR);
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     NULL, cleartext, sizeof(cleartext), plaintext,
                     sizeof(plaintext), ciphertext, tag), ==,
                   AES_ERR);
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, NULL, 1, plaintext, sizeof(plaintext), ciphertext,
                     tag), ==, AES_ERR);
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, cleartext, sizeof(cleartext), NULL, 1, ciphertext,
                     tag), ==, AES_ERR);
  munit_assert_int(AES_EAX_PRIME_encrypt(
                     key, NULL, 0, NULL, 0, NULL, tag), ==,
                   AES_OK);
  munit_assert_int(AES_EAX_PRIME_decrypt(
                     key, NULL, 0, NULL, 0, tag, NULL), ==,
                   AES_OK);
  return MUNIT_OK;
}

MunitResult test_eax_prime(const MunitParameter params[], void* data)
{
  return test_eax_prime_worked(params, data) == MUNIT_OK &&
                 test_eax_prime_c12_22(params, data) == MUNIT_OK ?
             MUNIT_OK : MUNIT_FAIL;
}

#endif /* EAX_PRIME */

#endif
