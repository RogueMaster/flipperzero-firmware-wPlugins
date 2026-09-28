/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PlatformIO / Arduino example: SHA-256 of "abc" against the
 * FIPS 180-4 known answer, streamed and one-shot.
 *
 * Arduino cores differ on setup()/loop() linkage:
 *   - ESP32 Arduino looks for C++-mangled symbols
 *   - AVR / STM32duino look for C linkage (extern "C")
 * Host builds (no ARDUINO) use main().
 */

#include <tiny_crypto/hash.h>
#include <string.h>

static int test_sha256_known_answer(void)
{
  static const uint8_t expected[TC_SHA256_DIGESTLEN] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
    0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
  };
  const uint8_t msg[3] = { 'a', 'b', 'c' };
  uint8_t out[TC_SHA256_DIGESTLEN];
  struct TC_SHA256_ctx ctx;

  if (TC_SHA256_digest(msg, sizeof(msg), out) != TC_OK)
    return 1;
  if (memcmp(out, expected, sizeof(out)) != 0)
    return 1;

  TC_SHA256_init(&ctx);
  TC_SHA256_update(&ctx, msg, 1);
  TC_SHA256_update(&ctx, msg + 1, 2);
  if (TC_SHA256_final(&ctx, out) != TC_OK)
    return 1;
  TC_SHA256_ctx_clear(&ctx);
  return memcmp(out, expected, sizeof(out)) == 0 ? 0 : 1;
}

#if defined(ARDUINO)

static void run_example(void)
{
  if (test_sha256_known_answer() != 0) {
    for (;;) {
      /* hang on failure */
    }
  }
}

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
/* Espressif Arduino cores call C++ setup()/loop() (mangled). */
void setup(void)
{
  run_example();
}

void loop(void)
{
}
#else
/* AVR, STM32duino, and similar cores resolve C-linkage setup()/loop(). */
extern "C" void setup(void)
{
  run_example();
}

extern "C" void loop(void)
{
}
#endif

#else /* !ARDUINO */

int main(void)
{
  return test_sha256_known_answer();
}

#endif
