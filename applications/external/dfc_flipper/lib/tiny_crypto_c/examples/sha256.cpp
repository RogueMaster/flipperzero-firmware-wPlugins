/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PlatformIO / Arduino example: SHA-256 of "abc" against the FIPS 180-4
 * known answer, one-shot and streamed.
 */

#include <tiny_crypto/hash.h>
#include <string.h>

#include "arduino_main.h"

static int sha256_known_answer(void)
{
  static const uint8_t expected[TC_SHA256_DIGESTLEN] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  const uint8_t msg[3] = {'a', 'b', 'c'};
  const TC_bytes message = {msg, sizeof(msg)};
  const TC_bytes head = {msg, 1};
  const TC_bytes tail = {msg + 1, sizeof(msg) - 1};
  uint8_t out[TC_SHA256_DIGESTLEN];
  struct TC_SHA256_ctx ctx;
  int failed;

  if (TC_SHA256_digest(message, out) != TC_OK || memcmp(out, expected, sizeof(out)) != 0)
    return 1;

  /* Streaming: every call can fail, and the context is cleared on every path. */
  failed = TC_SHA256_init(&ctx) != TC_OK || TC_SHA256_update(&ctx, head) != TC_OK ||
           TC_SHA256_update(&ctx, tail) != TC_OK || TC_SHA256_final(&ctx, out) != TC_OK;
  TC_SHA256_ctx_clear(&ctx);
  if (failed)
    return 1;
  return memcmp(out, expected, sizeof(out)) == 0 ? 0 : 1;
}

TC_EXAMPLE_ENTRY(sha256_known_answer)
