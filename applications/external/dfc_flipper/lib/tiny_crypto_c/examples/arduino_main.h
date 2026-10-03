/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Entry point for the PlatformIO / Arduino examples. TC_EXAMPLE_ENTRY(run)
 * turns an example function that returns 0 on success into the program's
 * entry point:
 *   - Espressif Arduino cores call C++ setup()/loop() (mangled names).
 *   - AVR, STM32duino and similar cores call C-linkage setup()/loop().
 *   - Host builds (no ARDUINO) use main() and return the example's status.
 * On a board a failed example stops in setup(), so a debugger or a missing
 * heartbeat shows the failure. */
#ifndef TINY_CRYPTO_EXAMPLE_ARDUINO_MAIN_H_
#define TINY_CRYPTO_EXAMPLE_ARDUINO_MAIN_H_

#if defined(ARDUINO)
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define TC_EXAMPLE_LINKAGE
#else
#define TC_EXAMPLE_LINKAGE extern "C"
#endif
#define TC_EXAMPLE_ENTRY(run)                                                                      \
  TC_EXAMPLE_LINKAGE void setup(void)                                                              \
  {                                                                                                \
    if (run() != 0)                                                                                \
      for (;;) {                                                                                   \
      }                                                                                            \
  }                                                                                                \
  TC_EXAMPLE_LINKAGE void loop(void)                                                               \
  {}
#else
#define TC_EXAMPLE_ENTRY(run)                                                                      \
  int main(void)                                                                                   \
  {                                                                                                \
    return run();                                                                                  \
  }
#endif

#endif
