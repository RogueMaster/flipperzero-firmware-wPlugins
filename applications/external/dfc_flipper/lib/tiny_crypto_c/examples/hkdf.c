/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>
#include <tiny_crypto/hkdf.h>

/* RFC 5869, Appendix A.1. Replace these values with protocol inputs. */
int main(void)
{
  uint8_t ikm[22];
  const uint8_t salt[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                          0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
  const uint8_t info[] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};
  const uint8_t expected[42] = {0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f,
                                0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a,
                                0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34,
                                0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65};
  uint8_t key[42];
  int success;

  /* The input keying material is a list of spans. A hybrid secret Z || T
   * from SP 800-56C revision 2 passes as two entries. */
  const TC_bytes secret[] = {{ikm, sizeof ikm}};
  memset(ikm, 0x0b, sizeof ikm);
  if (TC_HKDF_SHA256_derive((TC_bytes){salt, sizeof salt}, secret, 1, (TC_bytes){info, sizeof info},
                            (TC_buffer){key, sizeof key}) != TC_OK) {
    TC_secure_zero(ikm, sizeof ikm);
    return 1;
  }
  success = memcmp(key, expected, sizeof key) == 0;
  TC_secure_zero(ikm, sizeof ikm);
  TC_secure_zero(key, sizeof key);
  return success ? 0 : 1;
}
