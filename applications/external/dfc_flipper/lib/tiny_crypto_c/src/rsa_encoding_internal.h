/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_ENCODING_INTERNAL_H_
#define TC_RSA_ENCODING_INTERNAL_H_
#include <tiny_crypto/common.h>

/* prefix is the canonical DER DigestInfo header for the selected hash.
 * The caller supplies its matching fixed-length digest. */
static inline uint8_t tc_rsa_v15_byte(size_t index, size_t separator,
    const uint8_t* prefix, size_t prefix_length, const uint8_t* digest)
{
  if (!index || index == separator) return 0;
  if (index == 1) return 1;
  if (index < separator) return 0xff;
  index -= separator + 1;
  return index < prefix_length ? prefix[index] : digest[index - prefix_length];
}

static inline int tc_rsa_v15_size(size_t length, size_t prefix_length, size_t digest_length)
{
  return prefix_length && digest_length && length >= 11 &&
    prefix_length <= length - 11 && digest_length <= length - 11 - prefix_length;
}

/* RFC 8017 section 9.2. Inputs and output are disjoint; pointers cover the stated
 * lengths. Invalid sizing leaves output unchanged. No hash or RSA operation. */
static inline TC_status tc_rsa_v15_encode(uint8_t* out, size_t length,
    const uint8_t* prefix, size_t prefix_length, const uint8_t* digest, size_t digest_length)
{
  size_t separator;
  if (!out || !prefix || !digest || !tc_rsa_v15_size(length,prefix_length,digest_length))
    return TC_ERROR;
  separator = length - prefix_length - digest_length - 1;
  for (size_t i = 0; i < length; ++i)
    out[i] = tc_rsa_v15_byte(i,separator,prefix,prefix_length,digest);
  return TC_OK;
}

/* Compare the full representative, including padding and exact DER header.
 * Recovered representatives with invalid lengths or bytes return MISMATCH. */
static inline TC_status tc_rsa_v15_check(const uint8_t* encoded, size_t length,
    const uint8_t* prefix, size_t prefix_length, const uint8_t* digest, size_t digest_length)
{
  size_t separator;
  unsigned difference = 0;
  if (!encoded || !prefix || !digest) return TC_ERROR;
  if (!tc_rsa_v15_size(length,prefix_length,digest_length)) return TC_MISMATCH;
  separator = length - prefix_length - digest_length - 1;
  for (size_t i = 0; i < length; ++i)
    difference |= encoded[i] ^ tc_rsa_v15_byte(i,separator,prefix,prefix_length,digest);
  return difference ? TC_MISMATCH : TC_OK;
}
#endif
