/*
 * SPDX-License-Identifier: Unlicense
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 *
 * Opt-in CAVP response-file validation. This translation unit is part of the
 * test executable only; it is never linked into the library.
 */

#include "aes.h"
#include "munit.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CAVP_VECTOR_DIR
#define CAVP_VECTOR_DIR "test_vectors/cavp"
#endif

#if defined(AES_CAVP) && (AES_CAVP == 1)

#if (defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)) || (defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)) || \
    (defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1))
#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
static void cavp_initialize_sbox(void)
{
  AES_init_sbox();
}
#else
static void cavp_initialize_sbox(void)
{
}
#endif
#endif

enum cavp_mode { CAVP_ECB, CAVP_CBC, CAVP_OFB };

struct cavp_record
{
  uint8_t* key;
  size_t key_len;
  uint8_t* iv;
  size_t iv_len;
  uint8_t* plaintext;
  size_t plaintext_len;
  uint8_t* ciphertext;
  size_t ciphertext_len;
  size_t count;
};

static void cavp_record_clear(struct cavp_record* record)
{
  free(record->key);
  free(record->iv);
  free(record->plaintext);
  free(record->ciphertext);
  memset(record, 0, sizeof(*record));
}

static int cavp_hex_value(int c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int cavp_decode_hex(const char* text, uint8_t** output, size_t* length)
{
  const char* p = text;
  size_t digits = 0;
  size_t i = 0;
  uint8_t* result;

  while (*p != '\0' && *p != '\n' && *p != '\r')
  {
    if (cavp_hex_value((unsigned char)*p) >= 0)
      ++digits;
    ++p;
  }
  if ((digits & 1u) != 0)
    return 0;

  result = digits == 0 ? NULL : (uint8_t*)malloc(digits / 2u);
  if (digits != 0 && result == NULL)
    return 0;
  p = text;
  while (*p != '\0' && *p != '\n' && *p != '\r')
  {
    const int high = cavp_hex_value((unsigned char)*p);
    if (high >= 0)
    {
      const char* q = p + 1;
      int low;
      while (*q != '\0' && *q != '\n' && *q != '\r' &&
             cavp_hex_value((unsigned char)*q) < 0)
        ++q;
      if (*q == '\0' || *q == '\n' || *q == '\r')
      {
        free(result);
        return 0;
      }
      low = cavp_hex_value((unsigned char)*q);
      result[i++] = (uint8_t)((high << 4) | low);
      p = q;
    }
    ++p;
  }
  *output = result;
  *length = i;
  return 1;
}

static const char* cavp_value(const char* line)
{
  const char* p = strchr(line, '=');
  return p == NULL ? NULL : p + 1;
}

static int cavp_field(const char* line, const char* name,
                      uint8_t** output, size_t* length)
{
  const size_t name_len = strlen(name);
  const char* value;

  if (strncmp(line, name, name_len) != 0 ||
      (line[name_len] != ' ' && line[name_len] != '='))
    return 0;
  value = cavp_value(line);
  return value != NULL && cavp_decode_hex(value, output, length);
}

static int cavp_append(uint8_t** target, size_t* target_len,
                       const char* text)
{
  uint8_t* part = NULL;
  size_t part_len = 0;
  uint8_t* resized;

  if (!cavp_decode_hex(text, &part, &part_len))
    return 0;
  if (part_len > SIZE_MAX - *target_len)
  {
    free(part);
    return 0;
  }
  resized = (uint8_t*)realloc(*target, *target_len + part_len);
  if (part_len != 0 && resized == NULL)
  {
    free(part);
    return 0;
  }
  if (part_len != 0)
    memcpy(resized + *target_len, part, part_len);
  free(part);
  *target = resized;
  *target_len += part_len;
  return 1;
}

static void cavp_print_bytes(const char* label, const uint8_t* data,
                             size_t length)
{
  size_t i;
  fprintf(stderr, "  %s = ", label);
  for (i = 0; i < length; ++i)
    fprintf(stderr, "%02x", data[i]);
  fputc('\n', stderr);
}

static int cavp_compare(const char* file, size_t count, const char* field,
                        const uint8_t* actual, size_t actual_len,
                        const uint8_t* expected, size_t expected_len)
{
  if (actual_len == expected_len &&
      (actual_len == 0 || memcmp(actual, expected, actual_len) == 0))
    return 1;
  fprintf(stderr, "CAVP failure: %s Count=%lu field=%s\n", file,
          (unsigned long)count, field);
  cavp_print_bytes("expected", expected, expected_len);
  cavp_print_bytes("actual", actual, actual_len);
  return 0;
}

#if (defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1))
static void cavp_xor(uint8_t* dst, const uint8_t* src, size_t length)
{
  size_t i;
  for (i = 0; i < length; ++i)
    dst[i] ^= src[i];
}

static void cavp_ecb_block(const uint8_t* key, size_t key_len, int encrypt,
                           const uint8_t input[AES_BLOCKLEN],
                           uint8_t output[AES_BLOCKLEN])
{
  uint8_t block[AES_BLOCKLEN];

  (void)key_len;
  memcpy(block, input, sizeof(block));
  if (encrypt)
    AES_CAVP_encrypt_block(key, block);
  else
    AES_CAVP_decrypt_block(key, block);
  memcpy(output, block, sizeof(block));
}

static int cavp_standard_case(enum cavp_mode mode, const char* file,
                              int encrypt, const struct cavp_record* record)
{
  const uint8_t* input = encrypt ? record->plaintext : record->ciphertext;
  const uint8_t* expected = encrypt ? record->ciphertext : record->plaintext;
  const size_t input_len = encrypt ? record->plaintext_len : record->ciphertext_len;
  const size_t expected_len = encrypt ? record->ciphertext_len : record->plaintext_len;
  uint8_t* actual;

  if (input_len != expected_len || (mode == CAVP_ECB && (input_len & 15u) != 0))
  {
    fprintf(stderr, "CAVP malformed %s Count=%lu input=%lu expected=%lu key=%lu\n",
            file, (unsigned long)record->count, (unsigned long)input_len,
            (unsigned long)expected_len, (unsigned long)record->key_len);
    return 0;
  }
  actual = input_len == 0 ? NULL : (uint8_t*)malloc(input_len);
  if (input_len != 0 && actual == NULL)
    return 0;
  if (input_len != 0)
    memcpy(actual, input, input_len);

  if (mode == CAVP_ECB)
  {
#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
    struct AES_ctx ctx;
    size_t offset;
    AES_init_ctx(&ctx, record->key);
    for (offset = 0; offset < input_len; offset += AES_BLOCKLEN)
    {
      if (encrypt)
        AES_ECB_encrypt(&ctx, actual + offset);
      else
        AES_ECB_decrypt(&ctx, actual + offset);
    }
#else
    free(actual);
    return 0;
#endif
  }
  else if (mode == CAVP_CBC)
  {
#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, record->key, record->iv);
    if (encrypt)
      AES_CBC_encrypt(&ctx, actual, input_len);
    else
      AES_CBC_decrypt(&ctx, actual, input_len);
#else
    free(actual);
    return 0;
#endif
  }
  else
  {
#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, record->key, record->iv);
    AES_OFB_crypt(&ctx, actual, input_len);
#else
    free(actual);
    return 0;
#endif
  }

  if (!cavp_compare(file, record->count, encrypt ? "CIPHERTEXT" : "PLAINTEXT",
                    actual, input_len, expected, expected_len))
  {
    cavp_print_bytes("key", record->key, record->key_len);
    if (record->iv != NULL)
      cavp_print_bytes("iv", record->iv, record->iv_len);
    cavp_print_bytes(encrypt ? "plaintext" : "ciphertext", input, input_len);
    free(actual);
    return 0;
  }
  free(actual);
  return 1;
}

static void cavp_mct_block(enum cavp_mode mode, int encrypt,
                           const uint8_t* key, size_t key_len,
                           const uint8_t iv[AES_BLOCKLEN], int first,
                           const uint8_t input[AES_BLOCKLEN],
                           uint8_t output[AES_BLOCKLEN])
{
  uint8_t block[AES_BLOCKLEN];
  uint8_t stream[AES_BLOCKLEN];

  if (mode == CAVP_ECB)
  {
    cavp_ecb_block(key, key_len, encrypt, input, output);
    return;
  }
  if (mode == CAVP_CBC)
  {
    memcpy(block, input, sizeof(block));
    if (encrypt)
    {
      cavp_xor(block, iv, AES_BLOCKLEN);
      cavp_ecb_block(key, key_len, 1, block, output);
    }
    else
    {
      cavp_ecb_block(key, key_len, 0, block, output);
      cavp_xor(output, iv, AES_BLOCKLEN);
    }
    return;
  }
  memcpy(stream, first ? iv : input, AES_BLOCKLEN);
  cavp_ecb_block(key, key_len, 1, stream, stream);
  memcpy(output, input, AES_BLOCKLEN);
  cavp_xor(output, stream, AES_BLOCKLEN);
}

static int cavp_mct_intermediates(const char* response_file, size_t count,
                                  int encrypt,
                                  uint8_t expected[5][AES_BLOCKLEN])
{
  char path[512];
  char* line;
  FILE* file;
  size_t name_len = strlen(response_file);
  size_t current = (size_t)-1;
  size_t found = 0;
  int section_encrypt = 1;

  if (name_len < 4 || strcmp(response_file + name_len - 4, ".rsp") != 0)
    return 0;
  snprintf(path, sizeof(path), "%s/%.*s.txt", CAVP_VECTOR_DIR,
           (int)(name_len - 4), response_file);
  file = fopen(path, "r");
  if (file == NULL)
    return 0;
  line = (char*)malloc(4096);
  if (line == NULL)
  {
    fclose(file);
    return 0;
  }

  while (fgets(line, 4096, file) != NULL)
  {
    const char* value;
    uint8_t* decoded = NULL;
    size_t decoded_len = 0;

    if (strstr(line, "[ENCRYPT]") != NULL)
    {
      section_encrypt = 1;
      continue;
    }
    if (strstr(line, "[DECRYPT]") != NULL)
    {
      section_encrypt = 0;
      continue;
    }
    if (strncmp(line, "COUNT", 5) == 0)
    {
      value = cavp_value(line);
      current = value == NULL ? (size_t)-1 : (size_t)strtoull(value, NULL, 10);
      if (section_encrypt == encrypt && current == count)
        found = 0;
      else if (section_encrypt == encrypt && current > count && found == 5)
        break;
      continue;
    }
    if (section_encrypt != encrypt || current != count ||
        strstr(line, "Intermediate") == NULL ||
        strstr(line, encrypt ? "CIPHERTEXT" : "PLAINTEXT") == NULL)
      continue;
    value = cavp_value(line);
    if (value == NULL || !cavp_decode_hex(value, &decoded, &decoded_len) ||
        decoded_len != AES_BLOCKLEN || found == 5)
    {
      free(decoded);
      continue;
    }
    memcpy(expected[found++], decoded, AES_BLOCKLEN);
    free(decoded);
  }
  free(line);
  fclose(file);
  return found == 5;
}

static int cavp_mct_case(enum cavp_mode mode, const char* file, int encrypt,
                         const struct cavp_record* record)
{
  uint8_t key[32];
  uint8_t iv[AES_BLOCKLEN] = { 0 };
  uint8_t initial_iv[AES_BLOCKLEN] = { 0 };
  uint8_t input[AES_BLOCKLEN];
  uint8_t before_previous[AES_BLOCKLEN] = { 0 };
  uint8_t previous[AES_BLOCKLEN] = { 0 };
  uint8_t output[AES_BLOCKLEN] = { 0 };
  uint8_t ofb_state[AES_BLOCKLEN] = { 0 };
  uint8_t expected[AES_BLOCKLEN];
  uint8_t intermediate_expected[5][AES_BLOCKLEN];
  unsigned j;

  if (record->plaintext_len != AES_BLOCKLEN ||
      record->ciphertext_len != AES_BLOCKLEN || record->key_len > sizeof(key))
    return 0;
  memcpy(key, record->key, record->key_len);
  if (record->iv != NULL)
    memcpy(iv, record->iv, AES_BLOCKLEN);
  memcpy(initial_iv, iv, AES_BLOCKLEN);
  memcpy(ofb_state, iv, AES_BLOCKLEN);
  memcpy(input, encrypt ? record->plaintext : record->ciphertext,
         AES_BLOCKLEN);
  if (!cavp_mct_intermediates(file, record->count, encrypt,
                              intermediate_expected))
  {
    fprintf(stderr, "CAVP intermediate records missing: %s Count=%lu\n", file,
            (unsigned long)record->count);
    return 0;
  }

  for (j = 0; j < 1000; ++j)
  {
    const int first = j == 0;
    if (mode == CAVP_OFB)
    {
      AES_CAVP_encrypt_block(key, ofb_state);
      memcpy(output, input, AES_BLOCKLEN);
      cavp_xor(output, ofb_state, AES_BLOCKLEN);
    }
    else
      cavp_mct_block(mode, encrypt, key, record->key_len, iv, first, input,
                     output);
    if (mode == CAVP_CBC)
      memcpy(iv, encrypt ? output : input, sizeof(iv));
    memcpy(before_previous, previous, sizeof(before_previous));
    memcpy(previous, output, sizeof(previous));
    if (mode != CAVP_ECB && j == 0)
      memcpy(input, initial_iv, sizeof(input));
    else if (mode != CAVP_ECB)
      memcpy(input, before_previous, sizeof(input));
    else
      memcpy(input, previous, sizeof(input));
    if (j < 5 && memcmp(output, intermediate_expected[j], AES_BLOCKLEN) != 0)
    {
      fprintf(stderr,
              "CAVP intermediate failure: %s Count=%lu Intermediate=%u\n",
              file, (unsigned long)record->count, j);
      cavp_print_bytes("expected", intermediate_expected[j], AES_BLOCKLEN);
      cavp_print_bytes("actual", output, AES_BLOCKLEN);
      return 0;
    }
  }
  memcpy(expected, encrypt ? record->ciphertext : record->plaintext,
         AES_BLOCKLEN);
  if (!cavp_compare(file, record->count,
                    encrypt ? "CIPHERTEXT" : "PLAINTEXT", output,
                    AES_BLOCKLEN, expected, AES_BLOCKLEN))
    return 0;
  return 1;
}

static int cavp_run_block_file(enum cavp_mode mode, const char* directory,
                               const char* filename)
{
  char path[512];
  char* line;
  FILE* file;
  struct cavp_record record;
  int encrypt = 1;
  int mct;
  enum { FIELD_NONE, FIELD_KEY, FIELD_IV, FIELD_PT, FIELD_CT } active = FIELD_NONE;
  int ok = 1;

  snprintf(path, sizeof(path), "%s/%s/%s", CAVP_VECTOR_DIR, directory,
           filename);
  file = fopen(path, "r");
  if (file == NULL)
  {
    fprintf(stderr, "CAVP file not found: %s\n", path);
    return 0;
  }
  line = (char*)malloc(1024 * 1024);
  if (line == NULL)
  {
    fclose(file);
    return 0;
  }
  memset(&record, 0, sizeof(record));
  mct = strstr(filename, "MCT") != NULL;

  while (ok && fgets(line, 1024 * 1024, file) != NULL)
  {
    const char* value;
    if (strstr(line, "[DECRYPT]") != NULL)
    {
      encrypt = 0;
      continue;
    }
    if (strstr(line, "[ENCRYPT]") != NULL)
    {
      encrypt = 1;
      continue;
    }
    if (strncmp(line, "COUNT", 5) == 0)
    {
      value = cavp_value(line);
      record.count = value == NULL ? 0 : (size_t)strtoull(value, NULL, 10);
      active = FIELD_NONE;
      continue;
    }
    if (cavp_field(line, "KEY", &record.key, &record.key_len))
    {
      active = FIELD_KEY;
      continue;
    }
    if (cavp_field(line, "IV", &record.iv, &record.iv_len))
    {
      active = FIELD_IV;
      continue;
    }
    if (cavp_field(line, "PLAINTEXT", &record.plaintext,
                   &record.plaintext_len))
    {
      active = FIELD_PT;
      if (!encrypt)
      {
        ok = mct ? cavp_mct_case(mode, filename, encrypt, &record) :
                   cavp_standard_case(mode, filename, encrypt, &record);
        cavp_record_clear(&record);
        active = FIELD_NONE;
      }
      continue;
    }
    if (cavp_field(line, "CIPHERTEXT", &record.ciphertext,
                   &record.ciphertext_len))
    {
      active = FIELD_CT;
      if (encrypt)
      {
        ok = mct ? cavp_mct_case(mode, filename, encrypt, &record) :
                   cavp_standard_case(mode, filename, encrypt, &record);
        cavp_record_clear(&record);
        active = FIELD_NONE;
      }
      continue;
    }
    if (line[0] == ' ' || line[0] == '\t')
    {
      uint8_t** target = NULL;
      size_t* target_len = NULL;
      if (active == FIELD_PT) { target = &record.plaintext; target_len = &record.plaintext_len; }
      if (active == FIELD_CT) { target = &record.ciphertext; target_len = &record.ciphertext_len; }
      if (target != NULL && !cavp_append(target, target_len, line))
        ok = 0;
    }
  }
  cavp_record_clear(&record);
  free(line);
  fclose(file);
  return ok;
}
#endif

#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)
struct cavp_ccm_record
{
  uint8_t *key, *nonce, *aad, *payload, *ct;
  size_t key_len, nonce_len, aad_len, payload_len, ct_len;
  size_t count, tag_len;
  int expected_fail;
};

static size_t cavp_ccm_number(const char* line, const char* name)
{
  const char* p = strstr(line, name);
  return p == NULL || (p = strchr(p, '=')) == NULL ? 0 :
         (size_t)strtoull(p + 1, NULL, 10);
}

static void cavp_ccm_clear(struct cavp_ccm_record* record)
{
  free(record->key); free(record->nonce); free(record->aad);
  free(record->payload); free(record->ct);
  memset(record, 0, sizeof(*record));
}

static int cavp_run_ccm_case(const char* file,
                             const struct cavp_ccm_record* record,
                             int decrypt)
{
  uint8_t* output = record->payload_len == 0 ? NULL :
                    (uint8_t*)malloc(record->payload_len);
  uint8_t tag[16] = { 0 };
  int result;
  int ok;

  if (record->payload_len != 0 && output == NULL)
    return 0;
  if (decrypt)
  {
    if (record->ct_len < record->tag_len ||
        record->ct_len - record->tag_len != record->payload_len)
    {
      free(output);
      return 0;
    }
    result = AES_CCM_decrypt(record->key, record->nonce, record->nonce_len,
                             record->aad, record->aad_len, record->ct,
                             record->payload_len,
                             record->ct + record->payload_len,
                             record->tag_len, output);
    ok = record->expected_fail ? result == AES_ERR :
         result == AES_OK &&
         (record->payload_len == 0 ||
          memcmp(output, record->payload, record->payload_len) == 0);
  }
  else
  {
    result = AES_CCM_encrypt(record->key, record->nonce, record->nonce_len,
                             record->aad, record->aad_len, record->payload,
                             record->payload_len, output, tag,
                             record->tag_len);
    ok = !record->expected_fail && result == AES_OK &&
         record->ct_len == record->payload_len + record->tag_len &&
         (record->payload_len == 0 ||
          memcmp(output, record->ct, record->payload_len) == 0) &&
         memcmp(tag, record->ct + record->payload_len, record->tag_len) == 0;
  }
  if (!ok)
    fprintf(stderr, "CAVP failure: %s Count=%lu CCM result=%d expected=%s\n",
            file, (unsigned long)record->count, result,
            record->expected_fail ? "FAIL" : "PASS");
  free(output);
  return ok;
}

static int cavp_run_ccm_file(const char* filename)
{
  char path[512];
  char* line;
  FILE* file;
  struct cavp_ccm_record record;
  size_t configured_aad_len = 0, configured_payload_len = 0;
  size_t configured_nonce_len = 0, configured_tag_len = 0;
  const int decrypt = strstr(filename, "DVPT") != NULL;
  int ok = 1;

  snprintf(path, sizeof(path), "%s/ccm/%s", CAVP_VECTOR_DIR, filename);
  file = fopen(path, "r");
  if (file == NULL)
  {
    fprintf(stderr, "CAVP file not found: %s\n", path);
    return 0;
  }
  line = (char*)malloc(1024 * 1024);
  if (line == NULL) { fclose(file); return 0; }
  memset(&record, 0, sizeof(record));
  while (ok && fgets(line, 1024 * 1024, file) != NULL)
  {
    const char* value;
    if (strstr(line, "Alen =") != NULL || strstr(line, "Plen =") != NULL ||
        strstr(line, "Nlen =") != NULL || strstr(line, "Tlen =") != NULL)
    {
      if (strstr(line, "Alen =") != NULL)
        configured_aad_len = cavp_ccm_number(line, "Alen =");
      if (strstr(line, "Plen =") != NULL)
        configured_payload_len = cavp_ccm_number(line, "Plen =");
      if (strstr(line, "Nlen =") != NULL)
        configured_nonce_len = cavp_ccm_number(line, "Nlen =");
      if (strstr(line, "Tlen =") != NULL)
        configured_tag_len = cavp_ccm_number(line, "Tlen =");
      continue;
    }
    if (strncmp(line, "Count", 5) == 0)
    {
      free(record.aad); free(record.payload); free(record.ct);
      record.aad = record.payload = record.ct = NULL;
      record.aad_len = record.payload_len = record.ct_len = 0;
      value = cavp_value(line);
      record.count = value == NULL ? 0 : (size_t)strtoull(value, NULL, 10);
      record.expected_fail = 0;
      record.tag_len = configured_tag_len;
    }
    else if (strncmp(line, "Result = Fail", 13) == 0)
      record.expected_fail = 1;
    else if (strncmp(line, "Key", 3) == 0)
    {
      free(record.key); record.key = NULL; record.key_len = 0;
      if (!cavp_field(line, "Key", &record.key, &record.key_len)) ok = 0;
    }
    else if (strncmp(line, "Nonce", 5) == 0)
    {
      free(record.nonce); record.nonce = NULL; record.nonce_len = 0;
      if (!cavp_field(line, "Nonce", &record.nonce, &record.nonce_len)) ok = 0;
    }
    else if (cavp_field(line, "Adata", &record.aad, &record.aad_len)) { }
    else if (cavp_field(line, "Payload", &record.payload, &record.payload_len)) { }
    else if (cavp_field(line, "CT", &record.ct, &record.ct_len)) { }
    else if (line[0] == '\n' || line[0] == '\r')
    {
      struct cavp_ccm_record effective = record;
      effective.aad_len = configured_aad_len;
      effective.payload_len = configured_payload_len;
      effective.nonce_len = configured_nonce_len == 0 ? record.nonce_len : configured_nonce_len;
      if (effective.tag_len == 0 && record.ct_len >= effective.payload_len)
        effective.tag_len = record.ct_len - effective.payload_len;
      if (record.key != NULL && record.nonce != NULL && record.ct != NULL)
        ok = cavp_run_ccm_case(filename, &effective, decrypt);
      free(record.aad); free(record.payload); free(record.ct);
      record.aad = record.payload = record.ct = NULL;
      record.aad_len = record.payload_len = record.ct_len = 0;
    }
  }
  cavp_ccm_clear(&record);
  free(line);
  fclose(file);
  return ok;
}
#endif

#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
static int cavp_run_gcm_file(const char* filename)
{
  char path[512];
  char* line;
  FILE* file;
  uint8_t *key = NULL, *iv = NULL, *pt = NULL, *aad = NULL;
  uint8_t *ct = NULL, *tag = NULL;
  size_t key_len = 0, iv_len = 0, pt_len = 0, aad_len = 0;
  size_t ct_len = 0, tag_len = 0, count = 0;
  int decrypt = strstr(filename, "Decrypt") != NULL;
  int expected_fail = 0;
  int ok = 1;

  snprintf(path, sizeof(path), "%s/gcm/%s", CAVP_VECTOR_DIR, filename);
  file = fopen(path, "r");
  if (file == NULL)
  {
    fprintf(stderr, "CAVP file not found: %s\n", path);
    return 0;
  }
  line = (char*)malloc(1024 * 1024);
  if (line == NULL) { fclose(file); return 0; }
  while (ok && fgets(line, 1024 * 1024, file) != NULL)
  {
    const char* value;
    if (strncmp(line, "Count", 5) == 0)
    {
      free(key); free(iv); free(pt); free(aad); free(ct); free(tag);
      key = iv = pt = aad = ct = tag = NULL;
      key_len = iv_len = pt_len = aad_len = ct_len = tag_len = 0;
      value = cavp_value(line);
      count = value == NULL ? 0 : (size_t)strtoull(value, NULL, 10);
      expected_fail = 0;
    }
    else if (strncmp(line, "Key", 3) == 0)
    {
      free(key); key = NULL; key_len = 0;
      if (!cavp_field(line, "Key", &key, &key_len)) ok = 0;
    }
    else if (strncmp(line, "IV", 2) == 0)
    {
      free(iv); iv = NULL; iv_len = 0;
      if (!cavp_field(line, "IV", &iv, &iv_len)) ok = 0;
    }
    else if (cavp_field(line, "PT", &pt, &pt_len))
    {
      if (decrypt)
      {
        struct AES_GCM_ctx ctx;
        uint8_t* output = pt_len == 0 ? NULL : (uint8_t*)malloc(pt_len);
        int result;
        int init_result, aad_result, update_result, finish_result;
        if (ct_len != pt_len || (ct_len != 0 && output == NULL))
        {
          free(output);
          ok = 0;
          continue;
        }
        if (ct_len != 0)
          memcpy(output, ct, ct_len);
        cavp_initialize_sbox();
        init_result = AES_GCM_init(&ctx, key, iv, iv_len, tag_len);
        aad_result = init_result == AES_OK ?
                     AES_GCM_aad_update(&ctx, aad, aad_len) : AES_ERR;
        update_result = aad_result == AES_OK ?
                        AES_GCM_decrypt_update(&ctx, output, ct_len) : AES_ERR;
        finish_result = update_result == AES_OK ?
                        AES_GCM_decrypt_finish(&ctx, tag) : AES_ERR;
        result = finish_result;
        if (expected_fail)
          ok = result == AES_ERR;
        else
          ok = result == AES_OK && cavp_compare(filename, count, "PT", output, pt_len, pt, pt_len);
        if (!ok)
        {
          fprintf(stderr, "CAVP GCM failure: %s Count=%lu result=%d stages=%d/%d/%d/%d expected=%s\n",
                  filename, (unsigned long)count, result,
                  init_result, aad_result, update_result, finish_result,
                  expected_fail ? "FAIL" : "PASS");
          cavp_print_bytes("key", key, key_len);
          cavp_print_bytes("iv", iv, iv_len);
          cavp_print_bytes("tag", tag, tag_len);
        }
        free(output);
      }
    }
    else if (strncmp(line, "AAD", 3) == 0)
    {
      free(aad); aad = NULL; aad_len = 0;
      if (!cavp_field(line, "AAD", &aad, &aad_len)) ok = 0;
    }
    else if (strncmp(line, "CT", 2) == 0)
    {
      free(ct); ct = NULL; ct_len = 0;
      if (!cavp_field(line, "CT", &ct, &ct_len)) ok = 0;
    }
    else if (cavp_field(line, "Tag", &tag, &tag_len))
    {
      if (!decrypt)
      {
        struct AES_GCM_ctx ctx;
        uint8_t* output = pt_len == 0 ? NULL : (uint8_t*)malloc(pt_len);
        uint8_t actual_tag[16];
        int result;
        if (pt_len != 0 && output == NULL)
        {
          ok = 0;
          continue;
        }
        if (pt_len != 0)
          memcpy(output, pt, pt_len);
        cavp_initialize_sbox();
        result = AES_GCM_init(&ctx, key, iv, iv_len, tag_len);
        if (result == AES_OK) result = AES_GCM_aad_update(&ctx, aad, aad_len);
        if (result == AES_OK) result = AES_GCM_encrypt_update(&ctx, output, pt_len);
        if (result == AES_OK) result = AES_GCM_encrypt_finish(&ctx, actual_tag);
        ok = result == AES_OK && cavp_compare(filename, count, "CT", output, pt_len, ct, ct_len) &&
             cavp_compare(filename, count, "Tag", actual_tag, tag_len, tag, tag_len);
        if (!ok) fprintf(stderr, "CAVP GCM failure: %s Count=%lu\n", filename, (unsigned long)count);
        free(output);
      }
    }
    else if (strncmp(line, "FAIL", 4) == 0)
      expected_fail = 1;
  }
  free(key); free(iv); free(pt); free(aad); free(ct); free(tag);
  free(line);
  fclose(file);
  return ok;
}
#endif

static int cavp_run_all(void)
{
#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
  static const char* const ecb[] = {
    "ECBGFSbox128.rsp", "ECBGFSbox192.rsp", "ECBGFSbox256.rsp",
    "ECBKeySbox128.rsp", "ECBKeySbox192.rsp", "ECBKeySbox256.rsp",
    "ECBVarKey128.rsp", "ECBVarKey192.rsp", "ECBVarKey256.rsp",
    "ECBVarTxt128.rsp", "ECBVarTxt192.rsp", "ECBVarTxt256.rsp",
    "ECBMCT128.rsp", "ECBMCT192.rsp", "ECBMCT256.rsp",
    "ECBMMT128.rsp", "ECBMMT192.rsp", "ECBMMT256.rsp"
  };
#endif
#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
  static const char* const cbc[] = {
    "CBCGFSbox128.rsp", "CBCGFSbox192.rsp", "CBCGFSbox256.rsp",
    "CBCKeySbox128.rsp", "CBCKeySbox192.rsp", "CBCKeySbox256.rsp",
    "CBCVarKey128.rsp", "CBCVarKey192.rsp", "CBCVarKey256.rsp",
    "CBCVarTxt128.rsp", "CBCVarTxt192.rsp", "CBCVarTxt256.rsp",
    "CBCMCT128.rsp", "CBCMCT192.rsp", "CBCMCT256.rsp",
    "CBCMMT128.rsp", "CBCMMT192.rsp", "CBCMMT256.rsp"
  };
#endif
#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
  static const char* const ofb[] = {
    "OFBGFSbox128.rsp", "OFBGFSbox192.rsp", "OFBGFSbox256.rsp",
    "OFBKeySbox128.rsp", "OFBKeySbox192.rsp", "OFBKeySbox256.rsp",
    "OFBVarKey128.rsp", "OFBVarKey192.rsp", "OFBVarKey256.rsp",
    "OFBVarTxt128.rsp", "OFBVarTxt192.rsp", "OFBVarTxt256.rsp",
    "OFBMCT128.rsp", "OFBMCT192.rsp", "OFBMCT256.rsp",
    "OFBMMT128.rsp", "OFBMMT192.rsp", "OFBMMT256.rsp"
  };
#endif
#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)
  static const char* const ccm[] = {
    "DVPT128.rsp", "DVPT192.rsp", "DVPT256.rsp",
    "VADT128.rsp", "VADT192.rsp", "VADT256.rsp",
    "VNT128.rsp", "VNT192.rsp", "VNT256.rsp",
    "VPT128.rsp", "VPT192.rsp", "VPT256.rsp",
    "VTT128.rsp", "VTT192.rsp", "VTT256.rsp"
  };
#endif
#if (defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)) || (defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)) || \
    (defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1))
  size_t i;
#endif
  int ok = 1;

#if (defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)) || (defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)) || \
    (defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1))
#if defined(AES256) && (AES256 == 1)
  const char* key_suffix = "256";
#elif defined(AES192) && (AES192 == 1)
  const char* key_suffix = "192";
#else
  const char* key_suffix = "128";
#endif
#endif

#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
  for (i = 0; ok && i < sizeof(ecb) / sizeof(ecb[0]); ++i)
    if (strstr(ecb[i], key_suffix) != NULL) ok = cavp_run_block_file(CAVP_ECB, "ecb", ecb[i]);
#endif
#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
  for (i = 0; ok && i < sizeof(cbc) / sizeof(cbc[0]); ++i)
    if (strstr(cbc[i], key_suffix) != NULL) ok = cavp_run_block_file(CAVP_CBC, "cbc", cbc[i]);
#endif
#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
  for (i = 0; ok && i < sizeof(ofb) / sizeof(ofb[0]); ++i)
    if (strstr(ofb[i], key_suffix) != NULL) ok = cavp_run_block_file(CAVP_OFB, "ofb", ofb[i]);
#endif
#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)
  {
    static const char* const gcm[] = {
      "gcmDecrypt128.rsp", "gcmDecrypt192.rsp", "gcmDecrypt256.rsp",
      "gcmEncryptExtIV128.rsp", "gcmEncryptExtIV192.rsp", "gcmEncryptExtIV256.rsp"
    };
    for (i = 0; ok && i < sizeof(gcm) / sizeof(gcm[0]); ++i)
      if (strstr(gcm[i], key_suffix) != NULL) ok = cavp_run_gcm_file(gcm[i]);
  }
#endif
#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)
  for (i = 0; ok && i < sizeof(ccm) / sizeof(ccm[0]); ++i)
    if (strstr(ccm[i], key_suffix) != NULL)
      ok = cavp_run_ccm_file(ccm[i]);
#endif
  return ok;
}

MunitResult test_cavp(const MunitParameter params[], void* data)
{
  (void)params;
  (void)data;
#if (defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)) || (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)) || (defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)) || \
    (defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1))
  cavp_initialize_sbox();
#endif
  return cavp_run_all() ? MUNIT_OK : MUNIT_FAIL;
}

#else

MunitResult test_cavp(const MunitParameter params[], void* data)
{
  (void)params;
  (void)data;
  return MUNIT_OK;
}

#endif
