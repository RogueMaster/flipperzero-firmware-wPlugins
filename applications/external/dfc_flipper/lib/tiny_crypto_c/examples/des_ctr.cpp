/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PlatformIO / Arduino example: DES-CTR encrypt then decrypt.
 *
 * Arduino cores differ on setup()/loop() linkage:
 *   - ESP32 Arduino looks for C++-mangled symbols
 *   - AVR / STM32duino look for C linkage (extern "C")
 * Host builds (no ARDUINO) use main().
 */

#include <tiny_crypto/des.h>
#include <string.h>

static int test_des_ctr_roundtrip(void)
{
  struct TC_DES_ctx ctx;
  uint8_t key[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  uint8_t iv[8] = {0};
  uint8_t buf[16] = "hello DES CTR!!";
  uint8_t orig[16];

  memcpy(orig, buf, sizeof(buf));
  TC_DES_init_ctx_iv(&ctx, key, iv);
  if (TC_DES_CTR_crypt(&ctx, buf, sizeof(buf)) != TC_OK)
    return 1;
  TC_DES_ctx_set_iv(&ctx, iv);
  if (TC_DES_CTR_crypt(&ctx, buf, sizeof(buf)) != TC_OK)
    return 1;
  TC_DES_ctx_clear(&ctx);
  return memcmp(buf, orig, sizeof(buf)) == 0 ? 0 : 1;
}

#if defined(ARDUINO)

static void run_example(void)
{
  if (test_des_ctr_roundtrip() != 0) {
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
  return test_des_ctr_roundtrip();
}

#endif
