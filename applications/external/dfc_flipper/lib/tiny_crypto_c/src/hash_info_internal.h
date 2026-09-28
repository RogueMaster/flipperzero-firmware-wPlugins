/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_HASH_INFO_INTERNAL_H_
#define TC_HASH_INFO_INTERNAL_H_
#include <tiny_crypto/common.h>

typedef struct {
  TC_bytes oid;
  TC_bytes digest_info;
  size_t digest_length;
} tc_hash_info;

/* RFC 8017 section 9.2 DER prefixes, ending before the digest bytes.
 * Metadata is available even when a hash implementation is disabled. */
static inline int tc_hash_info_get(TC_hash_algorithm algorithm, tc_hash_info* out)
{
  static const uint8_t sha1[] = {0x30,0x21,0x30,9,6,5,0x2b,0x0e,3,2,0x1a,5,0,4,20};
  static const uint8_t sha2[][19] = {
    {0x30,0x2d,0x30,0x0d,6,9,0x60,0x86,0x48,1,0x65,3,4,2,4,5,0,4,28},
    {0x30,0x31,0x30,0x0d,6,9,0x60,0x86,0x48,1,0x65,3,4,2,1,5,0,4,32},
    {0x30,0x41,0x30,0x0d,6,9,0x60,0x86,0x48,1,0x65,3,4,2,2,5,0,4,48},
    {0x30,0x51,0x30,0x0d,6,9,0x60,0x86,0x48,1,0x65,3,4,2,3,5,0,4,64}
  };
  const uint8_t* prefix;
  size_t length;
  if (!out) return 0;
  if (algorithm == TC_HASH_SHA1) { prefix = sha1; length = sizeof sha1; }
  else if (algorithm >= TC_HASH_SHA224 && algorithm <= TC_HASH_SHA512) {
    prefix = sha2[algorithm - TC_HASH_SHA224]; length = sizeof sha2[0];
  } else return 0;
  out->oid.data = prefix + 6; out->oid.length = prefix[5];
  out->digest_info.data = prefix; out->digest_info.length = length;
  out->digest_length = prefix[length - 1];
  return 1;
}
#endif
