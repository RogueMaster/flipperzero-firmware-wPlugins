/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PlatformIO / Arduino example: SP 800-108 KBKDF in counter mode
 * over HMAC-SHA-256, checked against the Kdf108 cross-check vector. Build
 * with -DTC_ENABLE_HMAC=1 -DTC_ENABLE_KDF=1.
 *
 * Arduino cores differ on setup()/loop() linkage:
 *   - ESP32 Arduino looks for C++-mangled symbols
 *   - AVR / STM32duino look for C linkage (extern "C")
 * Host builds (no ARDUINO) use main().
 */

#include <tiny_crypto/kdf.h>
#include <string.h>

static int test_kbkdf_known_answer(void)
{
  static const uint8_t kdk[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
  };
  static const uint8_t label[] = "TestLabel";
  static const uint8_t context[] = "Vault:1|Box:2|Item:3";
  static const uint8_t expected[32] = {
    0xd3, 0x9d, 0x60, 0x1e, 0x90, 0xc9, 0xb0, 0xcb, 0x45, 0xb2, 0xe8, 0x41, 0x31, 0x3d, 0x0d, 0x41,
    0x72, 0xa1, 0xb3, 0xc5, 0x2a, 0xa8, 0xd0, 0x49, 0x30, 0x2b, 0x40, 0x1a, 0xeb, 0x9e, 0xdf, 0xb6
  };
  const struct TC_KBKDF_params params = { TC_KBKDF_COUNTER_32, 0, 0 };
  uint8_t fixed[TC_KBKDF_FIXED_INPUT_LEN(sizeof(label) - 1, sizeof(context) - 1)];
  uint8_t out[32];

  if (TC_KBKDF_fixed_input(label, sizeof(label) - 1, context, sizeof(context) - 1,
                           sizeof(out), fixed, sizeof(fixed)) != TC_OK)
    return 1;
  if (TC_KBKDF_HMAC_SHA256_counter(kdk, sizeof(kdk), &params, NULL, 0,
                                   fixed, sizeof(fixed), out, sizeof(out)) != TC_OK)
    return 1;
  return memcmp(out, expected, sizeof(out)) == 0 ? 0 : 1;
}

#if defined(ARDUINO)

static void run_example(void)
{
  if (test_kbkdf_known_answer() != 0) {
    for (;;) {
      /* hang on failure */
    }
  }
}

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
void setup(void)
{
  run_example();
}

void loop(void)
{
}
#else
extern "C" void setup(void)
{
  run_example();
}

extern "C" void loop(void)
{
}
#endif

#else

int main(void)
{
  return test_kbkdf_known_answer();
}

#endif
