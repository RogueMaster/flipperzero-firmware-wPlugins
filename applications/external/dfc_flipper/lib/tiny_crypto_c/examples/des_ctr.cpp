/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PlatformIO / Arduino example: DES-CTR encrypt then decrypt. Build with
 * -DTC_ENABLE_DES=1. Single DES is a legacy algorithm; use it only where a
 * protocol requires it.
 */

#include <tiny_crypto/des.h>
#include <string.h>

#include "arduino_main.h"

static int des_ctr_roundtrip(void)
{
  static const uint8_t key[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  static const uint8_t iv[8] = {0};
  uint8_t buf[16] = "hello DES CTR!!";
  uint8_t orig[16];
  struct TC_DES_ctx ctx;
  int failed;

  memcpy(orig, buf, sizeof(buf));
  /* Encrypt, then reset the IV and apply the same keystream to decrypt. */
  failed = TC_DES_init(&ctx, TC_bytes{key, TC_DES_KEYLEN}) != TC_OK ||
           TC_DES_set_iv(&ctx, TC_bytes{iv, TC_DES_BLOCKLEN}) != TC_OK ||
           TC_DES_CTR_crypt(&ctx, TC_buffer{buf, sizeof(buf)}) != TC_OK ||
           TC_DES_set_iv(&ctx, TC_bytes{iv, TC_DES_BLOCKLEN}) != TC_OK ||
           TC_DES_CTR_crypt(&ctx, TC_buffer{buf, sizeof(buf)}) != TC_OK;
  TC_DES_ctx_clear(&ctx);
  if (failed)
    return 1;
  return memcmp(buf, orig, sizeof(buf)) == 0 ? 0 : 1;
}

TC_EXAMPLE_ENTRY(des_ctr_roundtrip)
