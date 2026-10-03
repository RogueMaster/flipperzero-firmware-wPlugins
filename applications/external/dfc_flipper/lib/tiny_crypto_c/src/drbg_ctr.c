/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CTR_DRBG with AES (SP 800-90A section 10.2.1) and Block_Cipher_df
 * (section 10.3.2). The working state is Key and V, where V is a 128-bit
 * counter incremented over its full width. seedlen is the key length plus
 * 16 bytes. BCC reuses the shared CBC-MAC core. */
#include <tiny_crypto/drbg.h>

#if TC_DRBG_HAVE_CTR
#include <string.h>
#include "drbg_internal.h"
#include "aes_mac_core_internal.h"
#include "internal.h"

enum { BLOCK = 16, MAX_SEED = 48 };

TC_DRBG_result tc_drbg_ctr_parameters(uint8_t key_bytes, int derivation_function,
                                      tc_drbg_parameters* out)
{
  if (key_bytes != 16 && key_bytes != 24 && key_bytes != 32)
    return TC_DRBG_ARGUMENT;
  memset(out, 0, sizeof *out);
  out->strength_bits = (uint16_t)(key_bytes * 8u);
  out->seed_bytes = (uint8_t)(key_bytes + BLOCK);
  out->output_bytes = BLOCK;
  out->key_bytes = key_bytes;
  out->uses_nonce = derivation_function != 0;
  out->input_is_seed = derivation_function == 0;
  return TC_DRBG_OK;
}

static int encrypt(const TC_AES_dynamic_key* key, uint8_t block[BLOCK])
{
  return tc_aes_cipher_rounds((state_t*)block, key->round_key, key->rounds) == TC_OK;
}

/* CTR_DRBG_Update (10.2.1.2): provided is seedlen bytes. */
static TC_DRBG_result update(TC_DRBG* drbg, const uint8_t* provided)
{
  const size_t seed_bytes = drbg->seed_bytes, key_bytes = drbg->key_bytes;
  uint8_t temp[MAX_SEED];
  size_t offset, i;
  int ok = 1;

  for (offset = 0; ok && offset < seed_bytes; offset += BLOCK) {
    tc_internal_increment_be(drbg->state.ctr.v, BLOCK);
    memcpy(temp + offset, drbg->state.ctr.v, BLOCK);
    ok = encrypt(&drbg->state.ctr.key, temp + offset);
  }
  for (i = 0; ok && i < seed_bytes; ++i)
    temp[i] ^= provided[i];
  if (ok)
    ok = TC_AES_dynamic_key_init(&drbg->state.ctr.key, (TC_bytes){temp, key_bytes}) == TC_OK;
  if (ok)
    memcpy(drbg->state.ctr.v, temp + key_bytes, BLOCK);
  TC_secure_zero(temp, sizeof temp);
  return ok ? TC_DRBG_OK : TC_DRBG_ERROR;
}

/* BCC (10.3.3) of IV || S, where S = L || N || input || 0x80 || zero padding,
 * into chain. The shared CBC-MAC core zero-pads the final block. */
static int bcc(const tc_block_cipher* cipher, uint32_t index, const uint8_t header[8],
               const TC_bytes* parts, size_t count, uint8_t chain[BLOCK])
{
  static const uint8_t marker = 0x80;
  uint8_t iv[BLOCK] = {0};
  uint8_t block[BLOCK];
  uint8_t used = 0;
  size_t i;
  int ok;

  tc_internal_store_be32(iv, index);
  memset(chain, 0, BLOCK);
  ok = tc_mac_cbc_update(cipher, chain, block, &used, iv, BLOCK, 0) == TC_OK &&
       tc_mac_cbc_update(cipher, chain, block, &used, header, 8, 0) == TC_OK;
  for (i = 0; ok && i < count; ++i)
    ok = tc_mac_cbc_update(cipher, chain, block, &used, parts[i].data, parts[i].length, 0) == TC_OK;
  ok = ok && tc_mac_cbc_update(cipher, chain, block, &used, &marker, 1, 0) == TC_OK &&
       tc_mac_cbc_pad(cipher, chain, block, &used) == TC_OK;
  TC_secure_zero(block, sizeof block);
  return ok;
}

/* Block_Cipher_df (10.3.2): out = seedlen bytes derived from the parts, using
 * the scratch key schedule for its fixed and derived keys. */
static TC_DRBG_result block_cipher_df(TC_DRBG* drbg, const TC_bytes* parts, size_t count,
                                      uint8_t* out)
{
  const size_t seed_bytes = drbg->seed_bytes, key_bytes = drbg->key_bytes;
  const size_t input_length = tc_drbg_parts_length(parts, count);
  uint8_t key[32], header[8], temp[MAX_SEED];
  tc_aes_block_key mac_key;
  tc_block_cipher cipher;
  size_t offset;
  uint32_t index;
  /* The envelope bounds every input by TC_DRBG_MAX_INPUT_BYTES, so L fits
   * its 32-bit field. */
  int ok = 1;

  /* Step 8: K = leftmost keylen bits of 0x00 0x01 ... 0x1F. */
  for (offset = 0; offset < sizeof key; ++offset)
    key[offset] = (uint8_t)offset;
  ok = ok && TC_AES_dynamic_key_init(&drbg->scratch.df_key, (TC_bytes){key, key_bytes}) == TC_OK;
  tc_internal_store_be32(header, (uint32_t)input_length);
  tc_internal_store_be32(header + 4, (uint32_t)seed_bytes);
  mac_key.round_key = drbg->scratch.df_key.round_key;
  mac_key.rounds = drbg->scratch.df_key.rounds;
  cipher = tc_aes_block_cipher(&mac_key);

  /* Steps 9 to 11: temp = BCC(K, 0 || S) || BCC(K, 1 || S) || ... */
  for (index = 0, offset = 0; ok && offset < key_bytes + BLOCK; ++index, offset += BLOCK)
    ok = bcc(&cipher, index, header, parts, count, temp + offset);

  /* Steps 12 to 15: K and X from temp, then out = E(K, X) || E(K, E(K, X)) ... */
  if (ok)
    ok = TC_AES_dynamic_key_init(&drbg->scratch.df_key, (TC_bytes){temp, key_bytes}) == TC_OK;
  memmove(temp, temp + key_bytes, BLOCK);
  for (offset = 0; ok && offset < seed_bytes; offset += BLOCK) {
    ok = encrypt(&drbg->scratch.df_key, temp);
    memcpy(out + offset, temp, seed_bytes - offset < BLOCK ? seed_bytes - offset : BLOCK);
  }
  TC_secure_zero(key, sizeof key);
  TC_secure_zero(temp, sizeof temp);
  TC_secure_zero(&drbg->scratch.df_key, sizeof drbg->scratch.df_key);
  return ok ? TC_DRBG_OK : TC_DRBG_ERROR;
}

/* Seed material from the parts. With the derivation function it is
 * Block_Cipher_df of all parts. Without it, the first part is the entropy
 * input and the remaining parts, right-padded with zeros, are XORed into it.
 * The envelope bounds those parts to seedlen. */
static TC_DRBG_result seed_material(TC_DRBG* drbg, const TC_bytes* parts, size_t count,
                                    uint8_t out[MAX_SEED])
{
  size_t offset = 0, i, j;
  if (drbg->derivation_function)
    return block_cipher_df(drbg, parts, count, out);
  memset(out, 0, MAX_SEED);
  for (i = 1; i < count; ++i)
    for (j = 0; j < parts[i].length; ++j)
      out[offset++] = parts[i].data[j];
  for (j = 0; j < drbg->seed_bytes; ++j)
    out[j] ^= j < parts[0].length ? parts[0].data[j] : 0u;
  return TC_DRBG_OK;
}

/* Instantiate (10.2.1.3) starts from Key = 0 and V = 0. Reseed (10.2.1.4)
 * keeps the state. Both then run Update(seed_material). */
TC_DRBG_result tc_drbg_ctr_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed)
{
  static const uint8_t zero_key[32] = {0};
  uint8_t seed[MAX_SEED];
  TC_DRBG_result result = seed_material(drbg, parts, count, seed);
  if (result == TC_DRBG_OK && !reseed) {
    memset(drbg->state.ctr.v, 0, BLOCK);
    if (TC_AES_dynamic_key_init(&drbg->state.ctr.key, (TC_bytes){zero_key, drbg->key_bytes}) !=
        TC_OK)
      result = TC_DRBG_ERROR;
  }
  if (result == TC_DRBG_OK)
    result = update(drbg, seed);
  TC_secure_zero(seed, sizeof seed);
  return result;
}

/* Generate (10.2.1.5). Additional input becomes seedlen bytes, through the
 * derivation function or by zero padding, and is used before and after the
 * output. Empty additional input is seedlen zero bytes for the final Update. */
TC_DRBG_result tc_drbg_ctr_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                    TC_bytes additional)
{
  uint8_t adjusted[MAX_SEED] = {0};
  uint8_t block[BLOCK];
  size_t offset = 0;
  TC_DRBG_result result = TC_DRBG_OK;

  if (additional.length != 0) {
    const TC_bytes parts[2] = {{NULL, 0}, additional};
    result = seed_material(drbg, drbg->derivation_function ? &additional : parts,
                           drbg->derivation_function ? 1 : 2, adjusted);
    if (result == TC_DRBG_OK)
      result = update(drbg, adjusted);
  }
  while (result == TC_DRBG_OK && offset < length) {
    const size_t take = length - offset < BLOCK ? length - offset : BLOCK;
    tc_internal_increment_be(drbg->state.ctr.v, BLOCK);
    memcpy(block, drbg->state.ctr.v, BLOCK);
    if (!encrypt(&drbg->state.ctr.key, block))
      result = TC_DRBG_ERROR;
    else
      memcpy(output + offset, block, take);
    offset += take;
  }
  if (result == TC_DRBG_OK)
    result = update(drbg, adjusted);
  TC_secure_zero(adjusted, sizeof adjusted);
  TC_secure_zero(block, sizeof block);
  return result;
}

#endif
