/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES-SIV deterministic authenticated encryption (RFC 5297). */
#include "aes_mac_core_internal.h"

#if TC_AES_ENABLE_SIV
/* AES-CMAC under the S2V key over one or two concatenated parts. */
static TC_status tc_aes_siv_cmac(const uint8_t* round_key, const uint8_t k1[TC_AES_BLOCKLEN],
                                 const uint8_t k2[TC_AES_BLOCKLEN], const TC_bytes* parts,
                                 size_t count, uint8_t out[TC_AES_BLOCKLEN])
{
  const tc_aes_block_key key = {round_key, TC_AES_FIXED_ROUNDS};
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  return tc_mac_cmac_parts(&cipher, NULL, parts, count, k1, k2, out);
}

static TC_status tc_aes_siv_s2v(const uint8_t* k1_round, const TC_bytes* ad, size_t ad_count,
                                TC_bytes final, uint8_t v[TC_AES_BLOCKLEN])
{
  const uint8_t* last = final.data;
  const size_t last_len = final.length;
  uint8_t d[TC_AES_BLOCKLEN];
  uint8_t tmp[TC_AES_BLOCKLEN];
  uint8_t last_block[TC_AES_BLOCKLEN];
  uint8_t k1[TC_AES_BLOCKLEN];
  uint8_t k2[TC_AES_BLOCKLEN];
  size_t i;
  const uint8_t zero[TC_AES_BLOCKLEN] = {0};
  TC_status status;

  /* Every S2V component uses the same CMAC key, so derive its subkeys once. */
  status = tc_aes_cmac_generate_subkeys(k1_round, TC_AES_FIXED_ROUNDS, k1, k2);
  if (status != TC_OK)
    goto done;
  status = tc_aes_siv_cmac(k1_round, k1, k2, &(TC_bytes){zero, TC_AES_BLOCKLEN}, 1, d);
  if (status != TC_OK)
    goto done;
  for (i = 0; i < ad_count; ++i) {
    uint8_t j;
    status = tc_aes_siv_cmac(k1_round, k1, k2, &ad[i], 1, tmp);
    if (status != TC_OK)
      goto done;
    tc_aes_gf128_double(d);
    for (j = 0; j < TC_AES_BLOCKLEN; ++j)
      d[j] ^= tmp[j];
  }

  if (last_len >= TC_AES_BLOCKLEN) {
    /* T = last xorend D = prefix || (suffix xor D); CMAC(T). */
    memcpy(last_block, last + (last_len - TC_AES_BLOCKLEN), TC_AES_BLOCKLEN);
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      last_block[i] ^= d[i];
    const TC_bytes parts[] = {{last, last_len - TC_AES_BLOCKLEN}, {last_block, TC_AES_BLOCKLEN}};
    status = tc_aes_siv_cmac(k1_round, k1, k2, parts, 2, v);
  } else {
    /* T = dbl(D) xor pad(last); single-block CMAC input. */
    uint8_t t[TC_AES_BLOCKLEN];
    uint8_t j;
    memcpy(t, d, TC_AES_BLOCKLEN);
    tc_aes_gf128_double(t);
    memset(tmp, 0, TC_AES_BLOCKLEN);
    if (last_len != 0 && last != NULL)
      memcpy(tmp, last, last_len);
    tmp[last_len] = 0x80;
    for (j = 0; j < TC_AES_BLOCKLEN; ++j)
      t[j] ^= tmp[j];
    status = tc_aes_siv_cmac(k1_round, k1, k2, &(TC_bytes){t, TC_AES_BLOCKLEN}, 1, v);
    TC_secure_zero(t, sizeof(t));
  }

done:
  TC_secure_zero(d, sizeof(d));
  TC_secure_zero(tmp, sizeof(tmp));
  TC_secure_zero(last_block, sizeof(last_block));
  TC_secure_zero(k1, sizeof(k1));
  TC_secure_zero(k2, sizeof(k2));
  return status;
}

static TC_status tc_aes_siv_ctr(const uint8_t* k2_round, const uint8_t v[TC_AES_BLOCKLEN],
                                const uint8_t* input, uint8_t* output, size_t length)
{
  /* RFC 5297 section 2.5 clears bit 63 and bit 31 of the synthetic IV and
   * increments the 128-bit counter. The shared loop in aes_mac.c explains
   * why it stays apart from the SP 800-38A core. */
  return tc_aes_mac_ctr_xor(k2_round, v, input, output, length, (tc_aes_mac_ctr_bits){8u, 12u, 1u});
}

static TC_status tc_aes_siv_crypt(const uint8_t* key, const TC_bytes* ad, size_t ad_count,
                                  TC_bytes input, TC_buffer output, uint8_t v[TC_AES_SIV_V_LEN],
                                  int decrypt)
{
  const size_t input_len = input.length;
  struct {
    struct TC_AES_key_ctx k1;
    struct TC_AES_key_ctx k2;
    uint8_t computed[TC_AES_BLOCKLEN];
  } st;
  size_t i;
  TC_status status = TC_ERROR;

  if (key == NULL || (ad_count != 0 && ad == NULL) || ad_count > TC_AES_SIV_MAX_AD || v == NULL ||
      !tc_aes_text_ok(input, output))
    return TC_ERROR;

  /* Decrypt runs S2V over the AD after writing candidate plaintext, so an AD
   * span inside the output would authenticate overwritten bytes. */
  for (i = 0; i < ad_count; ++i) {
    if (!tc_internal_span_valid(ad[i].data, ad[i].length) ||
        !tc_internal_ranges_disjoint(ad[i].data, ad[i].length, output.data, input_len))
      return TC_ERROR;
  }

  if (TC_AES_key_init(&st.k1, (TC_bytes){key, TC_AES_KEYLEN}) != TC_OK ||
      TC_AES_key_init(&st.k2, (TC_bytes){key + TC_AES_KEYLEN, TC_AES_KEYLEN}) != TC_OK)
    goto done;

  if (decrypt) {
    status = tc_aes_siv_ctr(st.k2.round_key, v, input.data, output.data, input_len);
    if (status == TC_OK)
      status = tc_aes_siv_s2v(st.k1.round_key, ad, ad_count, (TC_bytes){output.data, input_len},
                              st.computed);
    if (status == TC_OK)
      status =
          TC_ct_equal((TC_bytes){st.computed, TC_AES_BLOCKLEN}, (TC_bytes){v, TC_AES_BLOCKLEN});
  } else {
    status = tc_aes_siv_s2v(st.k1.round_key, ad, ad_count, input, v);
    if (status == TC_OK)
      status = tc_aes_siv_ctr(st.k2.round_key, v, input.data, output.data, input_len);
  }

done:
  /* SIV decrypts before authenticating (RFC 5297 section 2.7). Any failure
   * discards the candidate plaintext or partial ciphertext. */
  if (status != TC_OK && input_len != 0)
    TC_secure_zero(output.data, input_len);
  TC_secure_zero(&st, sizeof(st));
  return status;
}

TC_status TC_AES_SIV_encrypt(TC_bytes key, const TC_bytes* ad, size_t ad_count, TC_bytes plaintext,
                             TC_buffer v, TC_buffer ciphertext)
{
  uint8_t local_v[TC_AES_SIV_V_LEN];
  TC_status status;

  if (key.length != TC_AES_SIV_KEYLEN || v.data == NULL || v.capacity < TC_AES_SIV_V_LEN)
    return TC_ERROR;
  /*
   * V is written after ciphertext. If they overlap, the post-encrypt copy
   * would clobber ciphertext (exact or partial). Stage V for pt alias only.
   */
  if (!tc_internal_ranges_disjoint(v.data, TC_AES_SIV_V_LEN, ciphertext.data, plaintext.length))
    return TC_ERROR;
  status = tc_aes_siv_crypt(key.data, ad, ad_count, plaintext, ciphertext, local_v, 0);
  if (status == TC_OK)
    memcpy(v.data, local_v, TC_AES_SIV_V_LEN);
  TC_secure_zero(local_v, sizeof(local_v));
  return status;
}

TC_status TC_AES_SIV_decrypt(TC_bytes key, const TC_bytes* ad, size_t ad_count, TC_bytes v,
                             TC_bytes ciphertext, TC_buffer plaintext)
{
  uint8_t local_v[TC_AES_SIV_V_LEN];
  TC_status status;

  if (key.length != TC_AES_SIV_KEYLEN || v.data == NULL || v.length != TC_AES_SIV_V_LEN)
    return TC_ERROR;
  if (!tc_internal_ranges_disjoint(v.data, TC_AES_SIV_V_LEN, plaintext.data, ciphertext.length))
    return TC_ERROR;
  memcpy(local_v, v.data, TC_AES_SIV_V_LEN);
  status = tc_aes_siv_crypt(key.data, ad, ad_count, ciphertext, plaintext, local_v, 1);
  TC_secure_zero(local_v, sizeof(local_v));
  return status;
}

#endif /* SIV */
