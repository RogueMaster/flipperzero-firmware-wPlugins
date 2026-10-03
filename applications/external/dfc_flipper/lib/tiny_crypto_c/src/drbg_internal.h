/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Interface between the SP 800-90A section 9 envelope (drbg.c) and the
 * mechanism algorithms of section 10 (drbg_hash.c, drbg_hmac.c, drbg_ctr.c).
 *
 * The envelope validates every argument, acquires entropy and manages the
 * reseed counter. The mechanisms implement only the update, instantiate,
 * reseed and generate algorithms over already checked inputs. Seed material
 * arrives as ordered TC_bytes parts so the mechanisms read the caller's
 * buffers in place:
 *   instantiate: entropy input, nonce, personalization string
 *   reseed:      entropy input, additional input */
#ifndef TC_DRBG_INTERNAL_H_
#define TC_DRBG_INTERNAL_H_

#include <tiny_crypto/drbg.h>

#if TC_ENABLE_DRBG

/* Mechanism parameters that the envelope enforces. */
typedef struct {
  uint16_t strength_bits;
  uint8_t seed_bytes;    /* seedlen / 8 */
  uint8_t output_bytes;  /* outlen / 8 */
  uint8_t key_bytes;     /* CTR_DRBG AES key length, else 0 */
  uint8_t uses_nonce;    /* 0 only for CTR_DRBG without a derivation function */
  uint8_t input_is_seed; /* inputs are XORed into the seed, so at most seed_bytes */
} tc_drbg_parameters;

#if TC_DRBG_HAVE_HASH
TC_DRBG_result tc_drbg_hash_parameters(TC_hash_algorithm hash, tc_drbg_parameters* out);
/* reseed selects the section 10.1.1.3 form. */
TC_DRBG_result tc_drbg_hash_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed);
TC_DRBG_result tc_drbg_hash_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                     TC_bytes additional);
#endif

#if TC_DRBG_HAVE_HMAC
TC_DRBG_result tc_drbg_hmac_parameters(TC_hash_algorithm hash, tc_drbg_parameters* out);
TC_DRBG_result tc_drbg_hmac_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed);
TC_DRBG_result tc_drbg_hmac_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                     TC_bytes additional);
#endif

#if TC_DRBG_HAVE_CTR
TC_DRBG_result tc_drbg_ctr_parameters(uint8_t key_bytes, int derivation_function,
                                      tc_drbg_parameters* out);
TC_DRBG_result tc_drbg_ctr_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed);
TC_DRBG_result tc_drbg_ctr_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                    TC_bytes additional);
#endif

#if TC_DRBG_HAVE_HASH || TC_DRBG_HAVE_HMAC
/* Security strength of Hash_DRBG and HMAC_DRBG (SP 800-90A Table 2), or 0
 * for an unknown identifier. */
static inline uint16_t tc_drbg_hash_strength(TC_hash_algorithm hash)
{
  switch (hash) {
  case TC_HASH_SHA1:
    return 128;
  case TC_HASH_SHA224:
    return 192;
  case TC_HASH_SHA256:
  case TC_HASH_SHA384:
  case TC_HASH_SHA512:
    return 256;
  default:
    return 0;
  }
}
#endif

/* Total length of parts, or SIZE_MAX when the sum overflows. */
static inline size_t tc_drbg_parts_length(const TC_bytes* parts, size_t count)
{
  size_t total = 0, i;
  for (i = 0; i < count; ++i) {
    if (parts[i].length > SIZE_MAX - total)
      return SIZE_MAX;
    total += parts[i].length;
  }
  return total;
}

#endif
#endif
