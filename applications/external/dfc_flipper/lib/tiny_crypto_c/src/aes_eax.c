/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES-EAX (Bellare, Rogaway, Wagner) and EAX' (ANSI C12.22) authenticated
 * encryption. */
#include "aes_mac_core_internal.h"

#if TC_AES_ENABLE_EAX || TC_AES_ENABLE_EAX_PRIME

/* EAX constants D = dbl(L) and Q = dbl(D) with L = E_K(0), which are the
 * CMAC subkeys. EAX' (ANSI C12.22) defines its field values in the
 * reference implementation's little-endian byte order, so it uses the
 * byte-reversed doubling. */
static TC_status tc_aes_eax_constants(const struct TC_AES_key_ctx* aes, int prime,
                                      uint8_t d[TC_AES_BLOCKLEN], uint8_t q[TC_AES_BLOCKLEN])
{
  const tc_aes_block_key key = {aes->round_key, TC_AES_FIXED_ROUNDS};
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  return tc_mac_derive_subkeys(&cipher, 0x87, prime, d, q);
}

/* EAX OMAC with domain prefix [domain]_n when domain >= 0. A negative domain
 * selects the C12.22 CMAC' form, whose CBC chain starts from initial (D or
 * Q) without a prefix. */
static TC_status tc_aes_eax_cmac(const struct TC_AES_key_ctx* aes,
                                 const uint8_t initial[TC_AES_BLOCKLEN], int domain,
                                 const uint8_t* data, size_t length,
                                 const uint8_t complete_subkey[TC_AES_BLOCKLEN],
                                 const uint8_t partial_subkey[TC_AES_BLOCKLEN],
                                 uint8_t result[TC_AES_BLOCKLEN])
{
  uint8_t prefix[TC_AES_BLOCKLEN] = {0};
  const TC_bytes parts[] = {{prefix, TC_AES_BLOCKLEN}, {data, length}};
  const tc_aes_block_key key = {aes->round_key, TC_AES_FIXED_ROUNDS};
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  prefix[TC_AES_BLOCKLEN - 1u] = (uint8_t)domain;
  return domain >= 0
             ? tc_mac_cmac_parts(&cipher, NULL, parts, 2, complete_subkey, partial_subkey, result)
             : tc_mac_cmac_parts(&cipher, initial, parts + 1, 1, complete_subkey, partial_subkey,
                                 result);
}

/* CTR over the full 128-bit counter N', wrapping modulo 2^128. The shared
 * loop in aes_mac.c explains why it stays apart from the SP 800-38A core. */
static TC_status tc_aes_eax_ctr_xor(const struct TC_AES_key_ctx* aes,
                                    const uint8_t initial[TC_AES_BLOCKLEN], const uint8_t* input,
                                    uint8_t* output, size_t length, int prime)
{
  return tc_aes_mac_ctr_xor(aes->round_key, initial, input, output, length,
                            (tc_aes_mac_ctr_bits){1u, 3u, (uint8_t)prime});
}

#if TC_AES_ENABLE_EAX

/* Decrypt when expected_tag is set, otherwise encrypt and write output_tag.
 * Exactly one tag pointer is set. */
static TC_status tc_aes_eax_crypt(const uint8_t* key, TC_bytes nonce, TC_bytes aad,
                                  TC_bytes input_span, TC_buffer output_span,
                                  const uint8_t* expected_tag, uint8_t* output_tag, size_t tag_len)
{
  const uint8_t* input = input_span.data;
  const size_t input_len = input_span.length;
  uint8_t* output = output_span.data;
  const int decrypt = expected_tag != NULL;
  struct {
    struct TC_AES_key_ctx aes;
    uint8_t nonce_mac[TC_AES_BLOCKLEN];
    uint8_t header_mac[TC_AES_BLOCKLEN];
    uint8_t message_mac[TC_AES_BLOCKLEN];
    uint8_t full_tag[TC_AES_BLOCKLEN];
    uint8_t d[TC_AES_BLOCKLEN];
    uint8_t q[TC_AES_BLOCKLEN];
  } st;
  TC_status status = TC_ERROR;
  uint8_t i;

  if (key == NULL || !tc_internal_span_valid(nonce.data, nonce.length) ||
      !tc_internal_span_valid(aad.data, aad.length) || !tc_aes_text_ok(input_span, output_span) ||
      (expected_tag == NULL && output_tag == NULL) || tag_len == 0 || tag_len > TC_AES_BLOCKLEN ||
      !tc_internal_ranges_disjoint(output, input_len,
                                   decrypt ? (const void*)expected_tag : (const void*)output_tag,
                                   tag_len))
    return TC_ERROR;

  if (TC_AES_key_init(&st.aes, (TC_bytes){key, TC_AES_KEYLEN}) != TC_OK)
    goto done;
  if (tc_aes_eax_constants(&st.aes, 0, st.d, st.q) != TC_OK ||
      tc_aes_eax_cmac(&st.aes, NULL, 0, nonce.data, nonce.length, st.d, st.q, st.nonce_mac) !=
          TC_OK ||
      tc_aes_eax_cmac(&st.aes, NULL, 1, aad.data, aad.length, st.d, st.q, st.header_mac) != TC_OK)
    goto done;

  if (decrypt) {
    if (tc_aes_eax_cmac(&st.aes, NULL, 2, input, input_len, st.d, st.q, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.header_mac[i] ^ st.message_mac[i]);
    status = TC_ct_equal((TC_bytes){st.full_tag, tag_len}, (TC_bytes){expected_tag, tag_len});
    if (status == TC_OK) {
      /* EAX verifies before CTR decryption, so the caller's buffer receives
       * only authenticated plaintext. */
      status = tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 0);
    }
  } else {
    if (tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 0) != TC_OK ||
        tc_aes_eax_cmac(&st.aes, NULL, 2, output, input_len, st.d, st.q, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.header_mac[i] ^ st.message_mac[i]);
    memcpy(output_tag, st.full_tag, tag_len);
    status = TC_OK;
  }

done:
  /* One-shot AEAD failure rule: after the argument checks, any failure wipes
   * the text output. Decrypt writes plaintext only after the tag verifies. */
  if (status != TC_OK && input_len != 0)
    TC_secure_zero(output, input_len);
  TC_secure_zero(&st, sizeof(st));
  return status;
}

/* EAX tags are the leading 1..16 bytes of the OMAC sum. short_tag selects
 * the lengths below TC_MIN_TAG_LEN. */
static TC_status tc_aes_eax_encrypt_with_policy(const uint8_t* key, TC_bytes nonce, TC_bytes aad,
                                                TC_bytes plaintext, TC_buffer ciphertext,
                                                TC_buffer tag, int short_tag)
{
  if (!tc_internal_tag_length_allowed(tag.capacity, TC_AES_BLOCKLEN, short_tag))
    return TC_ERROR;
  return tc_aes_eax_crypt(key, nonce, aad, plaintext, ciphertext, NULL, tag.data, tag.capacity);
}

static TC_status tc_aes_eax_decrypt_with_policy(const uint8_t* key, TC_bytes nonce, TC_bytes aad,
                                                TC_bytes ciphertext, TC_bytes tag,
                                                TC_buffer plaintext, int short_tag)
{
  if (tag.data == NULL || !tc_internal_tag_length_allowed(tag.length, TC_AES_BLOCKLEN, short_tag))
    return TC_ERROR;
  return tc_aes_eax_crypt(key, nonce, aad, ciphertext, plaintext, tag.data, NULL, tag.length);
}

TC_status TC_AES_EAX_encrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_eax_encrypt_with_policy(key.data, nonce, aad, plaintext, ciphertext, tag, 0);
}

TC_status TC_AES_EAX_decrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_eax_decrypt_with_policy(key.data, nonce, aad, ciphertext, tag, plaintext, 0);
}

TC_status TC_AES_EAX_encrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes plaintext, TC_buffer ciphertext, TC_buffer tag)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_eax_encrypt_with_policy(key.data, nonce, aad, plaintext, ciphertext, tag, 1);
}

TC_status TC_AES_EAX_decrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes ciphertext, TC_bytes tag, TC_buffer plaintext)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_eax_decrypt_with_policy(key.data, nonce, aad, ciphertext, tag, plaintext, 1);
}

#endif /* EAX */

#if TC_AES_ENABLE_EAX_PRIME

/* Decrypt when expected_tag is set, otherwise encrypt and write output_tag.
 * Exactly one tag pointer is set. */
static TC_status tc_aes_eax_prime_crypt(const uint8_t* key, TC_bytes cleartext, TC_bytes input_span,
                                        TC_buffer output_span, const uint8_t* expected_tag,
                                        uint8_t* output_tag)
{
  const uint8_t* input = input_span.data;
  const size_t input_len = input_span.length;
  uint8_t* output = output_span.data;
  const int decrypt = expected_tag != NULL;
  struct {
    struct TC_AES_key_ctx aes;
    uint8_t d[TC_AES_BLOCKLEN];
    uint8_t q[TC_AES_BLOCKLEN];
    uint8_t nonce_mac[TC_AES_BLOCKLEN];
    uint8_t message_mac[TC_AES_BLOCKLEN];
    uint8_t full_tag[TC_AES_BLOCKLEN];
  } st;
  uint8_t i;
  TC_status status = TC_ERROR;

  if (key == NULL || !tc_internal_span_valid(cleartext.data, cleartext.length) ||
      !tc_aes_text_ok(input_span, output_span) || (expected_tag == NULL && output_tag == NULL) ||
      !tc_internal_ranges_disjoint(output, input_len,
                                   decrypt ? (const void*)expected_tag : (const void*)output_tag,
                                   TC_AES_EAX_PRIME_TAG_LEN))
    return TC_ERROR;

  if (TC_AES_key_init(&st.aes, (TC_bytes){key, TC_AES_KEYLEN}) != TC_OK)
    goto done;
  if (tc_aes_eax_constants(&st.aes, 1, st.d, st.q) != TC_OK ||
      tc_aes_eax_cmac(&st.aes, st.d, -1, cleartext.data, cleartext.length, st.d, st.q,
                      st.nonce_mac) != TC_OK)
    goto done;

  if (decrypt) {
    if (tc_aes_eax_cmac(&st.aes, st.q, -1, input, input_len, st.d, st.q, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.message_mac[i]);
    /* EAX' writes its four-byte tag in reverse order. Reuse message_mac as a
     * small comparison buffer after its full-block value has been consumed. */
    for (i = 0; i < TC_AES_EAX_PRIME_TAG_LEN; ++i)
      st.message_mac[i] = st.full_tag[TC_AES_BLOCKLEN - 1u - i];
    status = TC_ct_equal((TC_bytes){st.message_mac, TC_AES_EAX_PRIME_TAG_LEN},
                         (TC_bytes){expected_tag, TC_AES_EAX_PRIME_TAG_LEN});
    if (status == TC_OK) {
      status = tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 1);
    }
  } else {
    if (tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 1) != TC_OK ||
        tc_aes_eax_cmac(&st.aes, st.q, -1, output, input_len, st.d, st.q, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.message_mac[i]);
    for (i = 0; i < TC_AES_EAX_PRIME_TAG_LEN; ++i)
      output_tag[i] = st.full_tag[TC_AES_BLOCKLEN - 1u - i];
    status = TC_OK;
  }

done:
  /* One-shot AEAD failure rule: after the argument checks, any failure wipes
   * the text output. Decrypt writes plaintext only after the tag verifies. */
  if (status != TC_OK && input_len != 0)
    TC_secure_zero(output, input_len);
  TC_secure_zero(&st, sizeof(st));
  return status;
}

TC_status TC_AES_EAX_PRIME_encrypt(TC_bytes key, TC_bytes cleartext, TC_bytes plaintext,
                                   TC_buffer ciphertext, TC_buffer tag)
{
  if (key.length != TC_AES_KEYLEN || tag.capacity < TC_AES_EAX_PRIME_TAG_LEN)
    return TC_ERROR;
  return tc_aes_eax_prime_crypt(key.data, cleartext, plaintext, ciphertext, NULL, tag.data);
}

TC_status TC_AES_EAX_PRIME_decrypt(TC_bytes key, TC_bytes cleartext, TC_bytes ciphertext,
                                   TC_bytes tag, TC_buffer plaintext)
{
  if (key.length != TC_AES_KEYLEN || tag.data == NULL || tag.length != TC_AES_EAX_PRIME_TAG_LEN)
    return TC_ERROR;
  return tc_aes_eax_prime_crypt(key.data, cleartext, ciphertext, plaintext, tag.data, NULL);
}

#endif /* EAX_PRIME */

#endif /* EAX || EAX_PRIME */
