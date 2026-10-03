/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_AES_MAC_CORE_INTERNAL_H_
#define TC_AES_MAC_CORE_INTERNAL_H_
#include "aes_internal.h"
#include "mac_core_internal.h"

/* Left-shift in GF(2^128), poly x^128+x^7+x^2+x+1. */
static inline void tc_aes_gf128_double(uint8_t value[TC_AES_BLOCKLEN])
{
  tc_mac_gf_double(value, value, TC_AES_BLOCKLEN, 0x87);
}

/* Counter bytes whose top bit is cleared before CTR use: bytes 1 and 3 for
 * EAX', bytes 8 and 12 for SIV (RFC 5297 section 2.5). */
typedef struct {
  uint8_t first_clear_bit;
  uint8_t second_clear_bit;
  uint8_t enabled;
} tc_aes_mac_ctr_bits;

/* XOR length bytes of AES-CTR keystream from initial into output. */
TC_status tc_aes_mac_ctr_xor(const uint8_t* round_key, const uint8_t initial[TC_AES_BLOCKLEN],
                             const uint8_t* input, uint8_t* output, size_t length,
                             tc_aes_mac_ctr_bits bits);

/* Derive both final-block CMAC subkeys from AES_K(0). */
TC_status tc_aes_cmac_generate_subkeys(const uint8_t* round_key, uint8_t rounds,
                                       uint8_t k1[TC_AES_BLOCKLEN], uint8_t k2[TC_AES_BLOCKLEN]);
#endif
