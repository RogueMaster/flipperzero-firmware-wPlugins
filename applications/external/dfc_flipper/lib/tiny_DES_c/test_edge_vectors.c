/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#include <string.h>

#include "des.h"
#include "munit.h"

#if DES_ENABLE_ECB && DES_ENABLE_CBC && DES_ENABLE_CFB1 && DES_ENABLE_CFB8 && \
    DES_ENABLE_CFB64 && DES_ENABLE_OFB && DES_ENABLE_TDES

#include "edge_vectors.h"

static MunitResult test_edge_vectors(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;

  for (size_t i = 0; i < EDGE_VECTOR_COUNT; ++i)
  {
    const struct edge_vector* vector = &edge_vectors[i];
    uint8_t buffer[32];
    munit_assert(vector->len <= sizeof(buffer));
    memcpy(buffer, vector->msg, vector->len);

    if (vector->key_len == 8)
    {
      struct DES_ctx ctx;
      DES_init_ctx_iv(&ctx, vector->key, vector->iv);

      if (strcmp(vector->mode, "ECB") == 0)
        DES_ECB_encrypt(&ctx, buffer);
      else if (strcmp(vector->mode, "CBC") == 0)
        DES_CBC_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "CFB1") == 0)
        DES_CFB1_encrypt(&ctx, buffer, vector->bit_length);
      else if (strcmp(vector->mode, "CFB8") == 0)
        DES_CFB8_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "CFB64") == 0)
        DES_CFB64_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "OFB") == 0)
        DES_OFB_crypt(&ctx, buffer, vector->len);
      else
        munit_errorf("unknown DES edge-vector mode: %s", vector->mode);
    }
    else
    {
      struct DES3_ctx ctx;
      DES3_init_ctx_iv(&ctx, vector->key, vector->key_len, vector->iv);

      if (strcmp(vector->mode, "ECB") == 0)
        DES3_ECB_encrypt(&ctx, buffer);
      else if (strcmp(vector->mode, "CBC") == 0)
        DES3_CBC_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "CFB1") == 0)
        DES3_CFB1_encrypt(&ctx, buffer, vector->bit_length);
      else if (strcmp(vector->mode, "CFB8") == 0)
        DES3_CFB8_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "CFB64") == 0)
        DES3_CFB64_encrypt(&ctx, buffer, vector->len);
      else if (strcmp(vector->mode, "OFB") == 0)
        DES3_OFB_crypt(&ctx, buffer, vector->len);
      else
        munit_errorf("unknown 3DES edge-vector mode: %s", vector->mode);
    }

    size_t compare_len = vector->bit_length ? (vector->bit_length + 7) / 8 : vector->len;
    if (memcmp(buffer, vector->ct, compare_len) != 0)
      munit_errorf("edge vector %zu (%s) mismatch", i, vector->mode);
  }

  return MUNIT_OK;
}

MunitResult test_edge_vectors_suite(const MunitParameter params[], void* data)
{
  return test_edge_vectors(params, data);
}

#else

MunitResult test_edge_vectors_suite(const MunitParameter params[], void* data)
{
  (void) params;
  (void) data;
  return MUNIT_SKIP;
}

#endif
