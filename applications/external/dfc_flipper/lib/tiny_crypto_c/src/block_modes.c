/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CBC, CTR and OFB confidentiality modes (NIST SP 800-38A) shared by AES and
 * DES through a tc_block_cipher descriptor. */
#include <string.h>
#include "block_cipher_internal.h"

#if TC_BLOCK_NEED_CBC
TC_status tc_block_cbc_encrypt(const tc_block_cipher* cipher, uint8_t* iv, uint8_t* buf,
                               size_t length)
{
  const size_t width = cipher->block_size;
  size_t offset;
  /* C_j = CIPH_K(P_j XOR C_{j-1}) with C_0 = IV. */
  for (offset = 0; offset < length; offset += width) {
    tc_internal_xor(buf + offset, iv, width);
    if (cipher->encrypt(cipher->key, buf + offset) != TC_OK) {
      TC_secure_zero(buf, length);
      TC_secure_zero(iv, width);
      return TC_ERROR;
    }
    memcpy(iv, buf + offset, width);
  }
  return TC_OK;
}

TC_status tc_block_cbc_decrypt(const tc_block_cipher* cipher, uint8_t* iv, uint8_t* buf,
                               size_t length)
{
  const size_t width = cipher->block_size;
  uint8_t previous[TC_BLOCK_MAX];
  size_t offset;
  TC_status status = TC_OK;
  /* P_j = CIPH^-1_K(C_j) XOR C_{j-1}. previous keeps C_j for the next step. */
  for (offset = 0; offset < length; offset += width) {
    memcpy(previous, buf + offset, width);
    if (cipher->decrypt(cipher->key, buf + offset) != TC_OK) {
      TC_secure_zero(buf, length);
      TC_secure_zero(iv, width);
      status = TC_ERROR;
      break;
    }
    tc_internal_xor(buf + offset, iv, width);
    memcpy(iv, previous, width);
  }
  TC_secure_zero(previous, sizeof previous);
  return status;
}
#endif

#if TC_BLOCK_NEED_CTR
TC_status tc_block_ctr_crypt(const tc_block_cipher* cipher, const tc_block_ctr_state* state,
                             uint8_t* buf, size_t length)
{
  const size_t width = cipher->block_size;
  size_t used = *state->used;
  size_t i;

  /* Section 6.5: O_j = CIPH_K(T_j) and C_j = P_j XOR O_j. Cached keystream
   * bytes serve the start of the next call. */
  for (i = 0; i < length; ++i) {
    if (used == width) {
      memcpy(state->keystream, state->counter, width);
      if (cipher->encrypt(cipher->key, state->keystream) != TC_OK) {
        TC_secure_zero(buf, length);
        return TC_ERROR;
      }
      *state->exhausted |= tc_internal_increment_be(state->counter, width);
      used = 0;
    }
    buf[i] ^= state->keystream[used++];
  }
  *state->used = (uint8_t)used;
  return TC_OK;
}
#endif

#if TC_BLOCK_NEED_OFB
TC_status tc_block_ofb_crypt(const tc_block_cipher* cipher, uint8_t* feedback, uint8_t* used,
                             uint8_t* buf, size_t length)
{
  const size_t width = cipher->block_size;
  size_t position = *used;
  size_t i;

  /* Section 6.4: O_j = CIPH_K(O_{j-1}) with O_0 = IV. The output block is
   * the next input block, so feedback is encrypted in place. */
  for (i = 0; i < length; ++i) {
    if (position == width) {
      if (cipher->encrypt(cipher->key, feedback) != TC_OK) {
        TC_secure_zero(buf, length);
        return TC_ERROR;
      }
      position = 0;
    }
    buf[i] ^= feedback[position++];
  }
  *used = (uint8_t)position;
  return TC_OK;
}
#endif
