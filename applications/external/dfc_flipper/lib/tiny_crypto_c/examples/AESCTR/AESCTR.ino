/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/aes.h>
#include <string.h>

static bool self_test()
{
  const uint8_t key[16] = {0};
  const uint8_t iv[16] = {0};
  uint8_t data[] = "tiny_crypto_c";
  uint8_t original[sizeof data];
  struct TC_AES_ctx context;
  memcpy(original, data, sizeof data);
  if (TC_AES_init(&context, (TC_bytes){key, sizeof key}) != TC_OK ||
      TC_AES_set_iv(&context, (TC_bytes){iv, sizeof iv}) != TC_OK ||
      TC_AES_CTR_crypt(&context, (TC_buffer){data, sizeof data}) != TC_OK ||
      TC_AES_set_iv(&context, (TC_bytes){iv, sizeof iv}) != TC_OK ||
      TC_AES_CTR_crypt(&context, (TC_buffer){data, sizeof data}) != TC_OK) {
    TC_AES_ctx_clear(&context);
    return false;
  }
  TC_AES_ctx_clear(&context);
  return memcmp(data, original, sizeof data) == 0;
}

void setup()
{
  Serial.begin(115200);
  Serial.println(self_test() ? "AES-CTR self-test passed" : "AES-CTR self-test failed");
}

void loop()
{}
