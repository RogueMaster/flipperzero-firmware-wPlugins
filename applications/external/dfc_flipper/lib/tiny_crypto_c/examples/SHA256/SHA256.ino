/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/hash.h>
#include <string.h>

void setup()
{
  static const uint8_t expected[TC_SHA256_DIGESTLEN] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  const uint8_t message[] = {'a', 'b', 'c'};
  uint8_t digest[TC_SHA256_DIGESTLEN];
  const bool passed = TC_SHA256_digest({message, sizeof message}, digest) == TC_OK &&
                      memcmp(digest, expected, sizeof digest) == 0;
  Serial.begin(115200);
  Serial.println(passed ? "SHA-256 self-test passed" : "SHA-256 self-test failed");
}

void loop()
{}
