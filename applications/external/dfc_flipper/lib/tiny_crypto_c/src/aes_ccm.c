/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aes_mac_core_internal.h"

#if TC_AES_ENABLE_CCM

#define TC_AES_CCM_MIN_NONCE_LEN 7u
#define TC_AES_CCM_MAX_NONCE_LEN 13u

static int tc_aes_ccm_tag_length_is_valid(size_t tag_len)
{
  return tag_len >= 4 && tag_len <= TC_AES_BLOCKLEN && (tag_len & 1u) == 0;
}

static unsigned tc_aes_ccm_length_field_size(size_t nonce_len)
{
  return (unsigned)(15u - nonce_len);
}

static int tc_aes_ccm_payload_length_is_valid(size_t nonce_len, size_t length)
{
  const unsigned q = tc_aes_ccm_length_field_size(nonce_len);

  if (q == sizeof(uint64_t))
    return 1;
  return (uint64_t)length <= ((UINT64_C(1) << (8u * q)) - 1u);
}

static void tc_aes_ccm_store_length(uint8_t* dst, uint64_t value, unsigned length)
{
  while (length-- != 0) {
    dst[length] = (uint8_t)value;
    value >>= 8;
  }
}

static void tc_aes_ccm_make_counter(uint8_t* counter, const uint8_t* nonce, size_t nonce_len,
                                    uint64_t value)
{
  const unsigned q = tc_aes_ccm_length_field_size(nonce_len);

  memset(counter, 0, TC_AES_BLOCKLEN);
  counter[0] = (uint8_t)(q - 1u);
  memcpy(counter + 1, nonce, nonce_len);
  tc_aes_ccm_store_length(counter + 1 + nonce_len, value, q);
}

/* The counter occupies the low q bytes of the block (SP 800-38C A.3). */
static void tc_aes_ccm_increment_counter(uint8_t* counter, unsigned q)
{
  /* The length checks keep the block count below 2^(8q), so no carry is lost. */
  (void)tc_internal_increment_be(counter + TC_AES_BLOCKLEN - q, q);
}

static TC_status tc_aes_ccm_xor_block(uint8_t* dst, size_t length, uint8_t* counter,
                                      const uint8_t* round_key)
{
  uint8_t stream[TC_AES_BLOCKLEN];
  size_t i;
  TC_status status;

  memcpy(stream, counter, TC_AES_BLOCKLEN);
  status = tc_aes_cipher((state_t*)stream, round_key);
  if (status != TC_OK)
    goto done;
  for (i = 0; i < length; ++i)
    dst[i] ^= stream[i];
done:
  TC_secure_zero(stream, sizeof(stream));
  return status;
}

/* Decrypt when expected_tag is set, otherwise encrypt and write output_tag.
 * Exactly one tag pointer is set. */
static TC_status tc_aes_ccm_crypt(const uint8_t* key, TC_bytes nonce_span, TC_bytes aad_span,
                                  TC_bytes input_span, TC_buffer output_span,
                                  const uint8_t* expected_tag, uint8_t* output_tag, size_t tag_len)
{
  const uint8_t* nonce = nonce_span.data;
  const size_t nonce_len = nonce_span.length;
  const uint8_t* aad = aad_span.data;
  const size_t aad_len = aad_span.length;
  const uint8_t* input = input_span.data;
  const size_t input_len = input_span.length;
  uint8_t* output = output_span.data;
  const int decrypt = expected_tag != NULL;
  /* Single workspace so stack use is predictable and wipe is one call. */
  struct {
    struct TC_AES_key_ctx aes;
    uint8_t mac[TC_AES_BLOCKLEN];
    uint8_t block[TC_AES_BLOCKLEN];
    uint8_t work[TC_AES_BLOCKLEN]; /* B0, then full tag */
    uint8_t counter[TC_AES_BLOCKLEN];
    uint8_t s0[TC_AES_BLOCKLEN];
    uint8_t plain[TC_AES_BLOCKLEN];
  } st;
  const tc_aes_block_key mac_key = {st.aes.round_key, TC_AES_FIXED_ROUNDS};
  const tc_block_cipher mac_cipher = tc_aes_block_cipher(&mac_key);
  uint8_t used = 0;
  size_t offset = 0;
  unsigned q;
  uint8_t i;
  TC_status status = TC_ERROR;

  if (key == NULL || nonce == NULL || (expected_tag == NULL && output_tag == NULL) ||
      !tc_internal_span_valid(aad, aad_len) || !tc_aes_text_ok(input_span, output_span) ||
      nonce_len < TC_AES_CCM_MIN_NONCE_LEN || nonce_len > TC_AES_CCM_MAX_NONCE_LEN ||
      !tc_aes_ccm_tag_length_is_valid(tag_len) ||
      !tc_aes_ccm_payload_length_is_valid(nonce_len, input_len) ||
      !tc_internal_ranges_disjoint(output, input_len,
                                   decrypt ? (const void*)expected_tag : (const void*)output_tag,
                                   tag_len))
    return TC_ERROR;

  memset(&st, 0, sizeof(st));
  q = tc_aes_ccm_length_field_size(nonce_len);
  st.work[0] = (uint8_t)((aad_len != 0 ? 0x40u : 0u) | (uint8_t)(((tag_len - 2u) / 2u) << 3) |
                         (uint8_t)(q - 1u));
  memcpy(st.work + 1, nonce, nonce_len);
  tc_aes_ccm_store_length(st.work + 1 + nonce_len, (uint64_t)input_len, q);

  if (TC_AES_key_init(&st.aes, (TC_bytes){key, TC_AES_KEYLEN}) != TC_OK)
    goto done;
  if (tc_mac_cbc_block(&mac_cipher, st.mac, st.work) != TC_OK)
    goto done;

  if (aad_len != 0) {
    uint8_t aad_header[10] = {0};
    size_t header_len;

    if (aad_len < 0xff00u) {
      header_len = 2;
      tc_aes_ccm_store_length(aad_header, (uint64_t)aad_len, 2);
    } else if ((uint64_t)aad_len <= UINT32_MAX) {
      header_len = 6;
      aad_header[0] = 0xff;
      aad_header[1] = 0xfe;
      tc_aes_ccm_store_length(aad_header + 2, (uint64_t)aad_len, 4);
    } else {
      header_len = 10;
      aad_header[0] = 0xff;
      aad_header[1] = 0xff;
      tc_aes_ccm_store_length(aad_header + 2, (uint64_t)aad_len, 8);
    }
    if (tc_mac_cbc_update(&mac_cipher, st.mac, st.block, &used, aad_header, header_len, 0) !=
            TC_OK ||
        tc_mac_cbc_update(&mac_cipher, st.mac, st.block, &used, aad, aad_len, 0) != TC_OK ||
        tc_mac_cbc_pad(&mac_cipher, st.mac, st.block, &used) != TC_OK)
      goto done;
  }

  tc_aes_ccm_make_counter(st.counter, nonce, nonce_len, 0);
  memcpy(st.s0, st.counter, TC_AES_BLOCKLEN);
  if (tc_aes_cipher((state_t*)st.s0, st.aes.round_key) != TC_OK)
    goto done;
  tc_aes_ccm_increment_counter(st.counter, q);

  while (offset < input_len) {
    const size_t length =
        (input_len - offset < TC_AES_BLOCKLEN) ? input_len - offset : TC_AES_BLOCKLEN;

    if (!decrypt &&
        tc_mac_cbc_update(&mac_cipher, st.mac, st.block, &used, input + offset, length, 0) != TC_OK)
      goto done;
    memset(st.plain, 0, TC_AES_BLOCKLEN);
    memcpy(st.plain, input + offset, length);
    if (decrypt && tc_aes_ccm_xor_block(st.plain, length, st.counter, st.aes.round_key) != TC_OK)
      goto done;
    if (decrypt &&
        tc_mac_cbc_update(&mac_cipher, st.mac, st.block, &used, st.plain, length, 0) != TC_OK)
      goto done;
    if (!decrypt) {
      if (tc_aes_ccm_xor_block(st.plain, length, st.counter, st.aes.round_key) != TC_OK)
        goto done;
      memcpy(output + offset, st.plain, length);
    }
    tc_aes_ccm_increment_counter(st.counter, q);
    offset += length;
  }
  if (tc_mac_cbc_pad(&mac_cipher, st.mac, st.block, &used) != TC_OK)
    goto done;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    st.work[i] = (uint8_t)(st.mac[i] ^ st.s0[i]);
  if (decrypt) {
    status = TC_ct_equal((TC_bytes){st.work, tag_len}, (TC_bytes){expected_tag, tag_len});
    if (status == TC_OK) {
      /* CBC-MAC needs plaintext. The first pass keeps each block private. The
       * second CTR pass writes the caller's buffer after authentication. */
      tc_aes_ccm_make_counter(st.counter, nonce, nonce_len, 0);
      tc_aes_ccm_increment_counter(st.counter, q);
      offset = 0;
      while (offset < input_len) {
        const size_t length =
            input_len - offset < TC_AES_BLOCKLEN ? input_len - offset : TC_AES_BLOCKLEN;
        memcpy(output + offset, input + offset, length);
        status = tc_aes_ccm_xor_block(output + offset, length, st.counter, st.aes.round_key);
        if (status != TC_OK)
          goto done;
        tc_aes_ccm_increment_counter(st.counter, q);
        offset += length;
      }
    }
  } else {
    memcpy(output_tag, st.work, tag_len);
    status = TC_OK;
  }
done:
  /* One-shot AEAD failure rule: after the argument checks, any failure wipes
   * the text output, including in-place ciphertext after a tag mismatch. */
  if (status != TC_OK && input_len != 0)
    TC_secure_zero(output, input_len);
  TC_secure_zero(&st, sizeof(st));
  return status;
}

/* SP 800-38C A.1 permits tags of 4..16 even bytes. short_tag selects the
 * lengths below TC_MIN_TAG_LEN. */
static TC_status tc_aes_ccm_encrypt_with_policy(const uint8_t* key, TC_bytes nonce, TC_bytes aad,
                                                TC_bytes plaintext, TC_buffer ciphertext,
                                                TC_buffer tag, int short_tag)
{
  if (!tc_internal_tag_length_allowed(tag.capacity, TC_AES_BLOCKLEN, short_tag))
    return TC_ERROR;
  return tc_aes_ccm_crypt(key, nonce, aad, plaintext, ciphertext, NULL, tag.data, tag.capacity);
}

static TC_status tc_aes_ccm_decrypt_with_policy(const uint8_t* key, TC_bytes nonce, TC_bytes aad,
                                                TC_bytes ciphertext, TC_bytes tag,
                                                TC_buffer plaintext, int short_tag)
{
  if (tag.data == NULL || !tc_internal_tag_length_allowed(tag.length, TC_AES_BLOCKLEN, short_tag))
    return TC_ERROR;
  return tc_aes_ccm_crypt(key, nonce, aad, ciphertext, plaintext, tag.data, NULL, tag.length);
}

TC_status TC_AES_CCM_encrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_ccm_encrypt_with_policy(key.data, nonce, aad, plaintext, ciphertext, tag, 0);
}

TC_status TC_AES_CCM_decrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_ccm_decrypt_with_policy(key.data, nonce, aad, ciphertext, tag, plaintext, 0);
}

TC_status TC_AES_CCM_encrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes plaintext, TC_buffer ciphertext, TC_buffer tag)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_ccm_encrypt_with_policy(key.data, nonce, aad, plaintext, ciphertext, tag, 1);
}

TC_status TC_AES_CCM_decrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes ciphertext, TC_bytes tag, TC_buffer plaintext)
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return tc_aes_ccm_decrypt_with_policy(key.data, nonce, aad, ciphertext, tag, plaintext, 1);
}

#endif
