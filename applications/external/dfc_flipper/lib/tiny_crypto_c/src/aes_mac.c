/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Shared AES MAC primitives: the counter-mode keystream used by EAX and SIV,
 * and CMAC subkey derivation (NIST SP 800-38B). */
#include "aes_mac_core_internal.h"

#if TC_AES_ENABLE_EAX || TC_AES_ENABLE_EAX_PRIME || TC_AES_ENABLE_SIV
/* EAX, EAX' and SIV share this keystream loop. It stays separate from the
 * SP 800-38A core in block_modes.c for two reasons. The AEAD counter wraps
 * modulo 2^128 (the EAX paper and RFC 5297 sections 2.5 and 6), while the core
 * requires callers to reject a request that would wrap the counter. The
 * AEAD calls also read input and write a separate output, while the core
 * works in place and would need an extra copy of the text. Both use a
 * full-block big-endian increment. */
TC_status tc_aes_mac_ctr_xor(const uint8_t* round_key, const uint8_t initial[TC_AES_BLOCKLEN],
                             const uint8_t* input, uint8_t* output, size_t length,
                             tc_aes_mac_ctr_bits bits)
{
  uint8_t counter[TC_AES_BLOCKLEN];
  uint8_t stream[TC_AES_BLOCKLEN];
  size_t offset = 0;
  TC_status status = TC_OK;

  memcpy(counter, initial, TC_AES_BLOCKLEN);
  if (bits.enabled) {
    counter[bits.first_clear_bit] &= 0x7fu;
    counter[bits.second_clear_bit] &= 0x7fu;
  }
  while (offset < length) {
    const size_t count = length - offset < TC_AES_BLOCKLEN ? length - offset : TC_AES_BLOCKLEN;
    memcpy(stream, counter, TC_AES_BLOCKLEN);
    status = tc_aes_cipher((state_t*)stream, round_key);
    if (status != TC_OK)
      break;
    for (size_t i = 0; i < count; ++i)
      output[offset + i] = (uint8_t)(input[offset + i] ^ stream[i]);
    tc_internal_increment_be(counter, TC_AES_BLOCKLEN);
    offset += count;
  }
  TC_secure_zero(counter, sizeof(counter));
  TC_secure_zero(stream, sizeof(stream));
  return status;
}
#endif

/* AES-CMAC (SP 800-38B) shared by public CMAC and SIV-S2V. */
#if TC_AES_ENABLE_CMAC || TC_AES_ENABLE_SIV || TC_AES_ENABLE_DYNAMIC
/* Derive both final-block subkeys from AES_K(0). */
TC_status tc_aes_cmac_generate_subkeys(const uint8_t* round_key, uint8_t rounds,
                                       uint8_t k1[TC_AES_BLOCKLEN], uint8_t k2[TC_AES_BLOCKLEN])
{
  const tc_aes_block_key key = {round_key, rounds};
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  return tc_mac_derive_subkeys(&cipher, 0x87, 0, k1, k2);
}
#endif
