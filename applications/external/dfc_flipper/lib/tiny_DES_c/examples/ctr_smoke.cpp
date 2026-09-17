/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 *
 * Minimal PlatformIO / Arduino-style smoke: DES-CTR encrypt then decrypt.
 *
 * Arduino cores differ on setup()/loop() linkage:
 *   - ESP32 Arduino looks for C++-mangled symbols
 *   - AVR / STM32duino look for C linkage (extern "C")
 * Host builds (no ARDUINO) use main().
 */

#include "des.h"
#include <string.h>

static int des_ctr_smoke(void)
{
  struct DES_ctx ctx;
  uint8_t key[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  uint8_t iv[8] = {0};
  uint8_t buf[16] = "hello DES CTR!!";
  uint8_t orig[16];

  memcpy(orig, buf, sizeof(buf));
  DES_init_ctx_iv(&ctx, key, iv);
  if (DES_CTR_crypt(&ctx, buf, sizeof(buf)) != DES_OK)
    return 1;
  DES_ctx_set_iv(&ctx, iv);
  if (DES_CTR_crypt(&ctx, buf, sizeof(buf)) != DES_OK)
    return 1;
  DES_ctx_clear(&ctx);
  return memcmp(buf, orig, sizeof(buf)) == 0 ? 0 : 1;
}

#if defined(ARDUINO)

static void arduino_smoke_entry(void)
{
  if (des_ctr_smoke() != 0) {
    for (;;) {
      /* hang on failure */
    }
  }
}

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
/* Espressif Arduino cores call C++ setup()/loop() (mangled). */
void setup(void)
{
  arduino_smoke_entry();
}

void loop(void)
{
}
#else
/* AVR, STM32duino, and similar cores resolve C-linkage setup()/loop(). */
extern "C" void setup(void)
{
  arduino_smoke_entry();
}

extern "C" void loop(void)
{
}
#endif

#else /* !ARDUINO */

int main(void)
{
  return des_ctr_smoke();
}

#endif
