/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aes_internal.h"

#if defined(TC_AES_ENABLE_CCM) && (TC_AES_ENABLE_CCM == 1)

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
  while (length-- != 0)
  {
    dst[length] = (uint8_t)value;
    value >>= 8;
  }
}

static TC_status tc_aes_ccm_mac_block(uint8_t* mac, const uint8_t* block,
                          const uint8_t* round_key)
{
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    mac[i] ^= block[i];
  return tc_aes_cipher((state_t*)mac, round_key);
}

static TC_status tc_aes_ccm_mac_absorb(uint8_t* mac, uint8_t* block, size_t* used,
                           const uint8_t* data, size_t length,
                           const uint8_t* round_key)
{
  while (length != 0)
  {
    const size_t available = TC_AES_BLOCKLEN - *used;
    const size_t count = length < available ? length : available;
    tc_aes_copy_bytes(block + *used, data, count);
    *used += count;
    data += count;
    length -= count;
    if (*used == TC_AES_BLOCKLEN)
    {
      if (tc_aes_ccm_mac_block(mac, block, round_key) != TC_OK) return TC_ERROR;
      *used = 0;
      memset(block, 0, TC_AES_BLOCKLEN);
    }
  }
  return TC_OK;
}

static TC_status tc_aes_ccm_mac_pad(uint8_t* mac, uint8_t* block, size_t* used,
                        const uint8_t* round_key)
{
  if (*used != 0)
  {
    memset(block + *used, 0, TC_AES_BLOCKLEN - *used);
    if (tc_aes_ccm_mac_block(mac, block, round_key) != TC_OK) return TC_ERROR;
    *used = 0;
    memset(block, 0, TC_AES_BLOCKLEN);
  }
  return TC_OK;
}

static void tc_aes_ccm_make_counter(uint8_t* counter, const uint8_t* nonce,
                             size_t nonce_len, uint64_t value)
{
  const unsigned q = tc_aes_ccm_length_field_size(nonce_len);

  memset(counter, 0, TC_AES_BLOCKLEN);
  counter[0] = (uint8_t)(q - 1u);
  tc_aes_copy_bytes(counter + 1, nonce, nonce_len);
  tc_aes_ccm_store_length(counter + 1 + nonce_len, value, q);
}

static void tc_aes_ccm_increment_counter(uint8_t* counter, unsigned q)
{
  unsigned i;

  for (i = TC_AES_BLOCKLEN; i > TC_AES_BLOCKLEN - q; --i)
  {
    const unsigned offset = i - 1u;
    if (++counter[offset] != 0)
      break;
  }
}

static TC_status tc_aes_ccm_xor_block(uint8_t* dst, size_t length, uint8_t* counter,
                          const uint8_t* round_key)
{
  uint8_t stream[TC_AES_BLOCKLEN];
  size_t i;
  TC_status status;

  tc_aes_copy_bytes(stream, counter, TC_AES_BLOCKLEN);
  status = tc_aes_cipher((state_t*)stream, round_key);
  if (status != TC_OK) goto done;
  for (i = 0; i < length; ++i)
    dst[i] ^= stream[i];
done:
#if TC_ZEROIZE
  TC_secure_zero(stream, sizeof(stream));
#endif
  return status;
}

static TC_status tc_aes_ccm_crypt(const uint8_t* key, const uint8_t* nonce,
                     size_t nonce_len, const uint8_t* aad, size_t aad_len,
                     const uint8_t* input, size_t input_len, uint8_t* output,
                     const uint8_t* expected_tag, uint8_t* output_tag,
                     size_t tag_len, int decrypt)
{
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
  size_t used = 0;
  size_t offset = 0;
  unsigned q;
  uint8_t i;
  TC_status status = TC_ERROR;
  int output_started = 0;

  if (key == NULL || nonce == NULL ||
      (decrypt ? expected_tag == NULL : output_tag == NULL) ||
      (aad_len != 0 && aad == NULL) ||
      (input_len != 0 && (input == NULL || output == NULL)) ||
      nonce_len < TC_AES_CCM_MIN_NONCE_LEN || nonce_len > TC_AES_CCM_MAX_NONCE_LEN ||
      !tc_aes_ccm_tag_length_is_valid(tag_len) ||
      !tc_aes_ccm_payload_length_is_valid(nonce_len, input_len) ||
      !tc_aes_buffers_ok(input, input_len, output, input_len) ||
      !tc_aes_buffers_disjoint(output, input_len,
                               decrypt ? (const void*)expected_tag
                                       : (const void*)output_tag,
                               tag_len))
    return TC_ERROR;

  memset(&st, 0, sizeof(st));
  q = tc_aes_ccm_length_field_size(nonce_len);
  st.work[0] = (uint8_t)((aad_len != 0 ? 0x40u : 0u) |
                         (uint8_t)(((tag_len - 2u) / 2u) << 3) |
                         (uint8_t)(q - 1u));
  tc_aes_copy_bytes(st.work + 1, nonce, nonce_len);
  tc_aes_ccm_store_length(st.work + 1 + nonce_len, (uint64_t)input_len, q);

  if (TC_AES_key_init(&st.aes, key) != TC_OK)
    goto done;
  if (tc_aes_ccm_mac_block(st.mac, st.work, st.aes.round_key) != TC_OK) goto done;

  if (aad_len != 0)
  {
    uint8_t aad_header[10] = { 0 };
    size_t header_len;

    if (aad_len < 0xff00u)
    {
      header_len = 2;
      tc_aes_ccm_store_length(aad_header, (uint64_t)aad_len, 2);
    }
    else if ((uint64_t)aad_len <= UINT32_MAX)
    {
      header_len = 6;
      aad_header[0] = 0xff;
      aad_header[1] = 0xfe;
      tc_aes_ccm_store_length(aad_header + 2, (uint64_t)aad_len, 4);
    }
    else
    {
      header_len = 10;
      aad_header[0] = 0xff;
      aad_header[1] = 0xff;
      tc_aes_ccm_store_length(aad_header + 2, (uint64_t)aad_len, 8);
    }
    if (tc_aes_ccm_mac_absorb(st.mac, st.block, &used, aad_header, header_len,
                            st.aes.round_key) != TC_OK ||
        tc_aes_ccm_mac_absorb(st.mac, st.block, &used, aad, aad_len, st.aes.round_key) != TC_OK ||
        tc_aes_ccm_mac_pad(st.mac, st.block, &used, st.aes.round_key) != TC_OK)
      goto done;
  }

  tc_aes_ccm_make_counter(st.counter, nonce, nonce_len, 0);
  tc_aes_copy_bytes(st.s0, st.counter, TC_AES_BLOCKLEN);
  if (tc_aes_cipher((state_t*)st.s0, st.aes.round_key) != TC_OK) goto done;
  tc_aes_ccm_increment_counter(st.counter, q);

  while (offset < input_len)
  {
    const size_t length = (input_len - offset < TC_AES_BLOCKLEN) ?
                          input_len - offset : TC_AES_BLOCKLEN;

    memset(st.plain, 0, TC_AES_BLOCKLEN);
    tc_aes_copy_bytes(st.plain, input + offset, length);
    if (decrypt && tc_aes_ccm_xor_block(st.plain, length, st.counter, st.aes.round_key) != TC_OK)
      goto done;
    if (tc_aes_ccm_mac_absorb(st.mac, st.block, &used, st.plain, length, st.aes.round_key) != TC_OK)
      goto done;
    if (!decrypt)
    {
      if (tc_aes_ccm_xor_block(st.plain, length, st.counter, st.aes.round_key) != TC_OK)
        goto done;
      output_started = 1;
      tc_aes_copy_bytes(output + offset, st.plain, length);
    }
    tc_aes_ccm_increment_counter(st.counter, q);
    offset += length;
  }
  if (tc_aes_ccm_mac_pad(st.mac, st.block, &used, st.aes.round_key) != TC_OK) goto done;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    st.work[i] = (uint8_t)(st.mac[i] ^ st.s0[i]);
  if (decrypt)
  {
    status = TC_ct_equal(st.work, expected_tag, tag_len);
    if (status == TC_OK)
    {
      /* CBC-MAC needs plaintext. The first pass keeps each block private;
       * only repeat CTR into the caller's buffer after authentication. */
      tc_aes_ccm_make_counter(st.counter, nonce, nonce_len, 0);
      tc_aes_ccm_increment_counter(st.counter, q);
      offset = 0;
      while (offset < input_len)
      {
        const size_t length = input_len - offset < TC_AES_BLOCKLEN ?
                              input_len - offset : TC_AES_BLOCKLEN;
        output_started = 1;
        tc_aes_copy_bytes(output + offset, input + offset, length);
        status = tc_aes_ccm_xor_block(output + offset, length, st.counter, st.aes.round_key);
        if (status != TC_OK) goto done;
        tc_aes_ccm_increment_counter(st.counter, q);
        offset += length;
      }
    }
    else
    {
      if (output != NULL && output == input)
        TC_secure_zero(output, input_len);
    }
  }
  else
  {
    tc_aes_copy_bytes(output_tag, st.work, tag_len);
    status = TC_OK;
  }
done:
  if (status != TC_OK && output_started && input_len != 0)
    TC_secure_zero(output, input_len);
#if TC_ZEROIZE
  TC_secure_zero(&st, sizeof(st));
#endif
  return status;
}

TC_status TC_AES_CCM_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len)
{
  return tc_aes_ccm_crypt(key, nonce, nonce_len, aad, aad_len, plaintext,
                   plaintext_len, ciphertext, NULL, tag, tag_len, 0);
}

TC_status TC_AES_CCM_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len, uint8_t* plaintext)
{
  return tc_aes_ccm_crypt(key, nonce, nonce_len, aad, aad_len, ciphertext,
                   ciphertext_len, plaintext, tag, NULL, tag_len, 1);
}

#endif
