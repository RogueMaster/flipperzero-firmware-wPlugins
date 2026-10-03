/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>
#include <tiny_crypto/aes_kw.h>

/* The KEK below holds TC_AES_KEYLEN bytes, which every build accepts. */
typedef char example_kek_supported[TC_AES_KW_KEK_LENGTH_SUPPORTED(TC_AES_KEYLEN) ? 1 : -1];

/* RFC 3394 section 4.1 at AES-128, then a KWP round trip of a 20-byte secret
 * and a rejected modified wrap. Replace the KEK with a stored secret. */
int main(void)
{
  uint8_t kek[TC_AES_KEYLEN];
  uint8_t content_key[16], secret[20];
  uint8_t wrapped[TC_AES_KW_WRAPPED_BYTES(sizeof content_key)];
  uint8_t unwrapped[TC_AES_KW_UNWRAPPED_BYTES(sizeof wrapped)] = {0};
  uint8_t padded[TC_AES_KWP_WRAPPED_BYTES(sizeof secret)];
  uint8_t recovered[TC_AES_KW_UNWRAPPED_BYTES(sizeof padded)] = {0};
  static const uint8_t expected[24] = {0x1f, 0xa6, 0x8b, 0x0a, 0x81, 0x12, 0xb4, 0x47,
                                       0xae, 0xf3, 0x4b, 0xd8, 0xfb, 0x5a, 0x7b, 0x82,
                                       0x9d, 0x3e, 0x86, 0x23, 0x71, 0xd2, 0xcf, 0xe5};
  const TC_bytes k = {kek, sizeof kek};
  size_t i, length = 0;
  int failed = 0;

  for (i = 0; i < sizeof kek; ++i)
    kek[i] = (uint8_t)i;
  for (i = 0; i < sizeof content_key; ++i)
    content_key[i] = (uint8_t)(0x11 * i);
  memset(secret, 0x42, sizeof secret);

  /* KW: the key data is a multiple of 8 bytes, at least 16. */
  if (TC_AES_KW_wrap(k, (TC_bytes){content_key, sizeof content_key},
                     (TC_buffer){wrapped, sizeof wrapped}) != TC_OK ||
      TC_AES_KW_unwrap(k, (TC_bytes){wrapped, sizeof wrapped},
                       (TC_buffer){unwrapped, sizeof unwrapped}) != TC_OK)
    failed = 1;
  else if (TC_AES_KEYLEN == 16 && memcmp(wrapped, expected, sizeof wrapped) != 0)
    failed = 1;
  else if (memcmp(unwrapped, content_key, sizeof content_key) != 0)
    failed = 1;

  /* KWP: any nonempty length. Unwrap reports the key data length. */
  if (!failed && (TC_AES_KWP_wrap(k, (TC_bytes){secret, sizeof secret},
                                  (TC_buffer){padded, sizeof padded}) != TC_OK ||
                  TC_AES_KWP_unwrap(k, (TC_bytes){padded, sizeof padded},
                                    (TC_buffer){recovered, sizeof recovered}, &length) != TC_OK ||
                  length != sizeof secret || memcmp(recovered, secret, sizeof secret) != 0))
    failed = 1;

  /* A modified wrap fails its integrity check and the output holds zeros. */
  if (!failed) {
    padded[5] ^= 0x01;
    if (TC_AES_KWP_unwrap(k, (TC_bytes){padded, sizeof padded},
                          (TC_buffer){recovered, sizeof recovered}, &length) != TC_MISMATCH)
      failed = 1;
    for (i = 0; i < sizeof recovered; ++i)
      failed |= recovered[i] != 0;
  }

  TC_secure_zero(kek, sizeof kek);
  TC_secure_zero(content_key, sizeof content_key);
  TC_secure_zero(secret, sizeof secret);
  TC_secure_zero(unwrapped, sizeof unwrapped);
  TC_secure_zero(recovered, sizeof recovered);
  return failed;
}
