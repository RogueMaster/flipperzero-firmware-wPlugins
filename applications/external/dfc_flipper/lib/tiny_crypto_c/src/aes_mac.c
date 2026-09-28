/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aes_internal.h"

#if (defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)) || \
    (defined(TC_AES_ENABLE_CMAC) && (TC_AES_ENABLE_CMAC == 1)) || \
    (defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1)) || TC_AES_ENABLE_DYNAMIC
/* Left-shift in GF(2^128), poly x^128+x^7+x^2+x+1. */
static void tc_aes_gf128_double(uint8_t value[TC_AES_BLOCKLEN])
{
  uint8_t carry = 0;
  unsigned i;

  for (i = TC_AES_BLOCKLEN; i > 0; --i)
  {
    const unsigned offset = i - 1u;
    const uint8_t next = (uint8_t)(value[offset] >> 7);
    value[offset] = (uint8_t)((value[offset] << 1) | carry);
    carry = next;
  }
  value[TC_AES_BLOCKLEN - 1u] ^= (uint8_t)(0x87u & (uint8_t)(0u - carry));
}
#endif

#if (defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)) || \
    (defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1))

#if defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1)
/* C12.22 defines EAX' field values in the reference implementation's
 * little-endian byte order, so its doubling shifts toward higher indexes and
 * applies the reduction constant to byte zero. */
static void tc_aes_eax_prime_double(uint8_t value[TC_AES_BLOCKLEN])
{
  uint8_t carry = 0;
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
  {
    const uint8_t next = (uint8_t)(value[i] >> 7);
    value[i] = (uint8_t)((value[i] << 1) | carry);
    carry = next;
  }
  value[0] ^= (uint8_t)(0x87u & (uint8_t)(0u - carry));
}
#endif

static TC_status tc_aes_eax_mac_block(uint8_t mac[TC_AES_BLOCKLEN],
                          const uint8_t block[TC_AES_BLOCKLEN],
                          const uint8_t* round_key)
{
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    mac[i] ^= block[i];
  return tc_aes_cipher((state_t*)mac, round_key);
}

#if defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)
static TC_status tc_aes_eax_key_constants(const struct TC_AES_key_ctx* aes,
                              uint8_t d[TC_AES_BLOCKLEN],
                              uint8_t q[TC_AES_BLOCKLEN])
{
  uint8_t l[TC_AES_BLOCKLEN] = { 0 };
  TC_status status = tc_aes_cipher((state_t*)l, aes->round_key);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(d, l, TC_AES_BLOCKLEN);
  tc_aes_gf128_double(d);
  tc_aes_copy_bytes(q, d, TC_AES_BLOCKLEN);
  tc_aes_gf128_double(q);
done:
#if TC_ZEROIZE
  TC_secure_zero(l, sizeof(l));
#endif
  return status;
}
#endif

#if defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1)
static TC_status tc_aes_eax_prime_key_constants(const struct TC_AES_key_ctx* aes,
                                    uint8_t d[TC_AES_BLOCKLEN],
                                    uint8_t q[TC_AES_BLOCKLEN])
{
  uint8_t l[TC_AES_BLOCKLEN] = { 0 };
  TC_status status = tc_aes_cipher((state_t*)l, aes->round_key);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(d, l, TC_AES_BLOCKLEN);
  tc_aes_eax_prime_double(d);
  tc_aes_copy_bytes(q, d, TC_AES_BLOCKLEN);
  tc_aes_eax_prime_double(q);
done:
#if TC_ZEROIZE
  TC_secure_zero(l, sizeof(l));
#endif
  return status;
}
#endif

/* CMAC with an optional EAX domain prefix. Passing domain < 0 implements the
 * C12.22 CMAC' form, whose CBC initial value is D or Q. */
static TC_status tc_aes_eax_cmac(const struct TC_AES_key_ctx* aes,
                     const uint8_t initial[TC_AES_BLOCKLEN], int domain,
                     const uint8_t* data, size_t length,
                     const uint8_t complete_subkey[TC_AES_BLOCKLEN],
                     const uint8_t partial_subkey[TC_AES_BLOCKLEN],
                     uint8_t result[TC_AES_BLOCKLEN])
{
  uint8_t mac[TC_AES_BLOCKLEN];
  uint8_t block[TC_AES_BLOCKLEN] = { 0 };
  size_t count;
  TC_status status;

  tc_aes_copy_bytes(mac, initial, TC_AES_BLOCKLEN);
  if (domain >= 0 && length == 0)
  {
    block[TC_AES_BLOCKLEN - 1u] = (uint8_t)domain;
    for (count = 0; count < TC_AES_BLOCKLEN; ++count)
      block[count] ^= complete_subkey[count];
  }
  else
  {
    if (domain >= 0)
    {
      block[TC_AES_BLOCKLEN - 1u] = (uint8_t)domain;
      status = tc_aes_eax_mac_block(mac, block, aes->round_key);
      if (status != TC_OK) goto done;
      memset(block, 0, TC_AES_BLOCKLEN);
    }

    while (length > TC_AES_BLOCKLEN)
    {
      status = tc_aes_eax_mac_block(mac, data, aes->round_key);
      if (status != TC_OK) goto done;
      data += TC_AES_BLOCKLEN;
      length -= TC_AES_BLOCKLEN;
    }

    if (length == TC_AES_BLOCKLEN)
    {
      tc_aes_copy_bytes(block, data, TC_AES_BLOCKLEN);
      for (count = 0; count < TC_AES_BLOCKLEN; ++count)
        block[count] ^= complete_subkey[count];
    }
    else
    {
      tc_aes_copy_bytes(block, data, length);
      block[length] = 0x80;
      for (count = 0; count < TC_AES_BLOCKLEN; ++count)
        block[count] ^= partial_subkey[count];
    }
  }
  status = tc_aes_eax_mac_block(mac, block, aes->round_key);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(result, mac, TC_AES_BLOCKLEN);
done:
#if TC_ZEROIZE
  TC_secure_zero(mac, sizeof(mac));
  TC_secure_zero(block, sizeof(block));
#endif
  return status;
}

#if defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)
static TC_status tc_aes_eax_omac(const struct TC_AES_key_ctx* aes, const uint8_t d[TC_AES_BLOCKLEN],
                     const uint8_t q[TC_AES_BLOCKLEN], uint8_t domain,
                     const uint8_t* data, size_t length,
                     uint8_t result[TC_AES_BLOCKLEN])
{
  uint8_t initial[TC_AES_BLOCKLEN] = { 0 };
  return tc_aes_eax_cmac(aes, initial, domain, data, length, d, q, result);
}
#endif

static TC_status tc_aes_eax_ctr_xor(const struct TC_AES_key_ctx* aes,
                        const uint8_t initial[TC_AES_BLOCKLEN],
                        const uint8_t* input, uint8_t* output, size_t length,
                        int prime)
{
  uint8_t counter[TC_AES_BLOCKLEN];
  uint8_t stream[TC_AES_BLOCKLEN];
  size_t offset = 0;
  TC_status status = TC_OK;

  tc_aes_copy_bytes(counter, initial, TC_AES_BLOCKLEN);
  if (prime)
  {
    counter[1] &= 0x7f;
    counter[3] &= 0x7f;
  }
  while (offset < length)
  {
    const size_t count = length - offset < TC_AES_BLOCKLEN ?
                         length - offset : TC_AES_BLOCKLEN;
    tc_aes_copy_bytes(stream, counter, TC_AES_BLOCKLEN);
    status = tc_aes_cipher((state_t*)stream, aes->round_key);
    if (status != TC_OK) break;
    for (size_t i = 0; i < count; ++i)
      output[offset + i] = (uint8_t)(input[offset + i] ^ stream[i]);
    tc_internal_increment_be(counter, TC_AES_BLOCKLEN);
    offset += count;
  }
#if TC_ZEROIZE
  TC_secure_zero(counter, sizeof(counter));
  TC_secure_zero(stream, sizeof(stream));
#endif
  return status;
}

#if defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)

static TC_status tc_aes_eax_crypt(const uint8_t* key, const uint8_t* nonce,
                     size_t nonce_len, const uint8_t* aad, size_t aad_len,
                     const uint8_t* input, size_t input_len, uint8_t* output,
                     const uint8_t* expected_tag, uint8_t* output_tag,
                     size_t tag_len, int decrypt)
{
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
  int output_started = 0;

  if (key == NULL || (nonce_len != 0 && nonce == NULL) ||
      (aad_len != 0 && aad == NULL) ||
      (input_len != 0 && (input == NULL || output == NULL)) ||
      (decrypt ? expected_tag == NULL : output_tag == NULL) ||
      tag_len < TC_AES_EAX_MIN_TAG_LEN || tag_len > TC_AES_BLOCKLEN ||
      !tc_aes_buffers_ok(input, input_len, output, input_len) ||
      !tc_aes_buffers_disjoint(output, input_len,
                               decrypt ? (const void*)expected_tag
                                       : (const void*)output_tag,
                               tag_len))
    return TC_ERROR;

  if (TC_AES_key_init(&st.aes, key) != TC_OK)
    goto done;
  if (tc_aes_eax_key_constants(&st.aes, st.d, st.q) != TC_OK ||
      tc_aes_eax_omac(&st.aes, st.d, st.q, 0, nonce, nonce_len, st.nonce_mac) != TC_OK ||
      tc_aes_eax_omac(&st.aes, st.d, st.q, 1, aad, aad_len, st.header_mac) != TC_OK)
    goto done;

  if (decrypt)
  {
    if (tc_aes_eax_omac(&st.aes, st.d, st.q, 2, input, input_len, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.header_mac[i] ^
                                 st.message_mac[i]);
    status = TC_ct_equal(st.full_tag, expected_tag, tag_len);
    if (status == TC_OK)
    {
      /* EAX verifies before CTR decryption, so unauthenticated plaintext is
       * never written to the caller's buffer. */
      output_started = 1;
      status = tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 0);
    }
  }
  else
  {
    output_started = 1;
    if (tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 0) != TC_OK ||
        tc_aes_eax_omac(&st.aes, st.d, st.q, 2, output, input_len, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.header_mac[i] ^
                                 st.message_mac[i]);
    tc_aes_copy_bytes(output_tag, st.full_tag, tag_len);
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

TC_status TC_AES_EAX_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len)
{
  return tc_aes_eax_crypt(key, nonce, nonce_len, aad, aad_len, plaintext,
                   plaintext_len, ciphertext, NULL, tag, tag_len, 0);
}

TC_status TC_AES_EAX_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len, uint8_t* plaintext)
{
  return tc_aes_eax_crypt(key, nonce, nonce_len, aad, aad_len, ciphertext,
                   ciphertext_len, plaintext, tag, NULL, tag_len, 1);
}

#endif /* EAX */

#if defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1)

static TC_status tc_aes_eax_prime_crypt(const uint8_t* key, const uint8_t* cleartext,
                           size_t cleartext_len, const uint8_t* input,
                           size_t input_len, uint8_t* output,
                           const uint8_t* expected_tag, uint8_t* output_tag,
                           int decrypt)
{
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
  int output_started = 0;

  if (key == NULL || (cleartext_len != 0 && cleartext == NULL) ||
      (input_len != 0 && (input == NULL || output == NULL)) ||
      (decrypt ? expected_tag == NULL : output_tag == NULL) ||
      !tc_aes_buffers_ok(input, input_len, output, input_len) ||
      !tc_aes_buffers_disjoint(output, input_len,
                               decrypt ? (const void*)expected_tag
                                       : (const void*)output_tag,
                               TC_AES_EAX_PRIME_TAG_LEN))
    return TC_ERROR;

  if (TC_AES_key_init(&st.aes, key) != TC_OK)
    goto done;
  if (tc_aes_eax_prime_key_constants(&st.aes, st.d, st.q) != TC_OK ||
      tc_aes_eax_cmac(&st.aes, st.d, -1, cleartext, cleartext_len, st.d, st.q,
                      st.nonce_mac) != TC_OK)
    goto done;

  if (decrypt)
  {
    if (tc_aes_eax_cmac(&st.aes, st.q, -1, input, input_len, st.d, st.q, st.message_mac) != TC_OK)
      goto done;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      st.full_tag[i] = (uint8_t)(st.nonce_mac[i] ^ st.message_mac[i]);
    /* EAX' writes its four-byte tag in reverse order. Reuse message_mac as a
     * small comparison buffer after its full-block value has been consumed. */
    for (i = 0; i < TC_AES_EAX_PRIME_TAG_LEN; ++i)
      st.message_mac[i] = st.full_tag[TC_AES_BLOCKLEN - 1u - i];
    status = TC_ct_equal(st.message_mac, expected_tag,
                         TC_AES_EAX_PRIME_TAG_LEN);
    if (status == TC_OK)
    {
      output_started = 1;
      status = tc_aes_eax_ctr_xor(&st.aes, st.nonce_mac, input, output, input_len, 1);
    }
  }
  else
  {
    output_started = 1;
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
  if (status != TC_OK && output_started && input_len != 0)
    TC_secure_zero(output, input_len);
#if TC_ZEROIZE
  TC_secure_zero(&st, sizeof(st));
#endif
  return status;
}

TC_status TC_AES_EAX_PRIME_encrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* plaintext,
                          size_t plaintext_len, uint8_t* ciphertext,
                          uint8_t tag[TC_AES_EAX_PRIME_TAG_LEN])
{
  return tc_aes_eax_prime_crypt(key, cleartext, cleartext_len, plaintext,
                         plaintext_len, ciphertext, NULL, tag, 0);
}

TC_status TC_AES_EAX_PRIME_decrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* ciphertext,
                          size_t ciphertext_len,
                          const uint8_t tag[TC_AES_EAX_PRIME_TAG_LEN],
                          uint8_t* plaintext)
{
  return tc_aes_eax_prime_crypt(key, cleartext, cleartext_len, ciphertext,
                         ciphertext_len, plaintext, tag, NULL, 1);
}

#endif /* EAX_PRIME */

#endif /* EAX || EAX_PRIME */

/* AES-CMAC (SP 800-38B) shared by public CMAC and SIV-S2V. */
#if TC_AES_ENABLE_CMAC || TC_AES_ENABLE_SIV || TC_AES_ENABLE_DYNAMIC

/* Derive both final-block subkeys from AES_K(0). */
static TC_status tc_aes_cmac_generate_subkeys(const uint8_t* round_key, uint8_t rounds,
                                         uint8_t k1[TC_AES_BLOCKLEN],
                                         uint8_t k2[TC_AES_BLOCKLEN])
{
  uint8_t l[TC_AES_BLOCKLEN] = { 0 };

  TC_status status = tc_aes_cipher_rounds((state_t*)l, round_key, rounds);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(k1, l, TC_AES_BLOCKLEN);
  tc_aes_gf128_double(k1);
  tc_aes_copy_bytes(k2, k1, TC_AES_BLOCKLEN);
  tc_aes_gf128_double(k2);
done:
#if TC_ZEROIZE
  TC_secure_zero(l, sizeof(l));
#endif
  return status;
}

#if defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1)
static TC_status tc_aes_cmac_concat(const uint8_t* round_key,
                            const uint8_t k1[TC_AES_BLOCKLEN],
                            const uint8_t k2[TC_AES_BLOCKLEN],
                            const uint8_t* a, size_t a_len,
                            const uint8_t* b, size_t b_len,
                            uint8_t out[TC_AES_BLOCKLEN])
{
  uint8_t mac[TC_AES_BLOCKLEN] = { 0 };
  uint8_t block[TC_AES_BLOCKLEN];
  size_t total = a_len + b_len;
  size_t pos = 0;
  uint8_t i;
  TC_status status;

  if (total == 0)
  {
    memset(block, 0, TC_AES_BLOCKLEN);
    block[0] = 0x80;
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      block[i] ^= k2[i];
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      mac[i] ^= block[i];
    status = tc_aes_cipher((state_t*)mac, round_key);
    if (status != TC_OK) goto done;
    tc_aes_copy_bytes(out, mac, TC_AES_BLOCKLEN);
    goto done;
  }

  while (total - pos > TC_AES_BLOCKLEN)
  {
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    {
      const size_t p = pos + i;
      const uint8_t byte = (p < a_len) ? a[p] : b[p - a_len];
      mac[i] ^= byte;
    }
    status = tc_aes_cipher((state_t*)mac, round_key);
    if (status != TC_OK) goto done;
    pos += TC_AES_BLOCKLEN;
  }

  {
    const size_t rem = total - pos;
    memset(block, 0, TC_AES_BLOCKLEN);
    for (i = 0; i < (uint8_t)rem; ++i)
    {
      const size_t p = pos + i;
      block[i] = (p < a_len) ? a[p] : b[p - a_len];
    }
    if (rem == TC_AES_BLOCKLEN)
    {
      for (i = 0; i < TC_AES_BLOCKLEN; ++i)
        block[i] ^= k1[i];
    }
    else
    {
      block[rem] = 0x80;
      for (i = 0; i < TC_AES_BLOCKLEN; ++i)
        block[i] ^= k2[i];
    }
  }

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    mac[i] ^= block[i];
  status = tc_aes_cipher((state_t*)mac, round_key);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(out, mac, TC_AES_BLOCKLEN);

done:
  (void)0;
#if TC_ZEROIZE
  TC_secure_zero(mac, sizeof(mac));
  TC_secure_zero(block, sizeof(block));
#endif
  return status;
}
#endif

#if defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1)
static TC_status tc_aes_cmac_with_subkeys(const uint8_t* round_key,
                              const uint8_t k1[TC_AES_BLOCKLEN],
                              const uint8_t k2[TC_AES_BLOCKLEN],
                              const uint8_t* data, size_t length,
                              uint8_t out[TC_AES_BLOCKLEN])
{
  return tc_aes_cmac_concat(round_key, k1, k2, data, length, NULL, 0, out);
}
#endif


#if TC_AES_ENABLE_CMAC || TC_AES_ENABLE_DYNAMIC
static TC_status tc_aes_cmac_absorb(const uint8_t* key, uint8_t rounds, uint8_t mac[16],
                               const uint8_t block[16])
{
  tc_internal_xor(mac, block, 16);
  return tc_aes_cipher_rounds((state_t*)mac, key, rounds);
}

static TC_status tc_aes_cmac_update(const uint8_t* key, uint8_t rounds, uint8_t mac[16],
    uint8_t buffer[16], uint8_t* used, const uint8_t* data, size_t length)
{
  /* Retain the last block until finalization chooses its subkey. */
  while (length) {
    size_t take;
    if (*used == 16) {
      if (tc_aes_cmac_absorb(key, rounds, mac, buffer) != TC_OK) return TC_ERROR;
      *used = 0;
    }
    if (!*used && length > 16) {
      if (tc_aes_cmac_absorb(key, rounds, mac, data) != TC_OK) return TC_ERROR;
      data += 16; length -= 16;
      continue;
    }
    take = 16u - *used;
    if (take > length) take = length;
    memcpy(buffer + *used, data, take);
    *used = (uint8_t)(*used + take);
    data += take; length -= take;
  }
  return TC_OK;
}

static TC_status tc_aes_cmac_final(const uint8_t* key, uint8_t rounds, uint8_t mac[16],
    uint8_t buffer[16], uint8_t used, const uint8_t k1[16], const uint8_t k2[16], uint8_t tag[16])
{
  if (used == 16) tc_internal_xor(buffer, k1, 16);
  else {
    memset(buffer + used, 0, 16u - used);
    buffer[used] = 0x80;
    tc_internal_xor(buffer, k2, 16);
  }
  if (tc_aes_cmac_absorb(key, rounds, mac, buffer) != TC_OK) return TC_ERROR;
  memcpy(tag, mac, 16);
  return TC_OK;
}
#endif

#if TC_AES_ENABLE_DYNAMIC
TC_status TC_AES_dynamic_CMAC_init(TC_AES_dynamic_CMAC* ctx, const uint8_t* key, size_t length)
{
  if (!ctx || !tc_internal_ranges_disjoint(ctx, sizeof *ctx, key, length) ||
      TC_AES_dynamic_key_init(&ctx->key, key, length) != TC_OK) return TC_ERROR;
  if (tc_aes_cmac_generate_subkeys(ctx->key.round_key, ctx->key.rounds, ctx->k1, ctx->k2) != TC_OK) {
    TC_AES_dynamic_CMAC_clear(ctx);
    return TC_ERROR;
  }
  memset(ctx->mac, 0, 16); memset(ctx->buffer, 0, 16); ctx->used = 0;
  return TC_OK;
}

static int tc_aes_dynamic_cmac_valid(const TC_AES_dynamic_CMAC* ctx)
{
  return ctx && ctx->used <= 16 &&
    (ctx->key.rounds == 10 || ctx->key.rounds == 12 || ctx->key.rounds == 14);
}

TC_status TC_AES_dynamic_CMAC_update(TC_AES_dynamic_CMAC* ctx, const uint8_t* data, size_t length)
{
  if (!tc_aes_dynamic_cmac_valid(ctx) || (!data && length) ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, data, length)) return TC_ERROR;
  if (tc_aes_cmac_update(ctx->key.round_key, ctx->key.rounds, ctx->mac,
                        ctx->buffer, &ctx->used, data, length) != TC_OK) {
    TC_AES_dynamic_CMAC_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_dynamic_CMAC_final(TC_AES_dynamic_CMAC* ctx, uint8_t tag[16])
{
  TC_status status;
  if (!tc_aes_dynamic_cmac_valid(ctx) || !tag ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, tag, 16)) return TC_ERROR;
  status = tc_aes_cmac_final(ctx->key.round_key, ctx->key.rounds, ctx->mac,
                           ctx->buffer, ctx->used, ctx->k1, ctx->k2, tag);
  TC_AES_dynamic_CMAC_clear(ctx);
  return status;
}

void TC_AES_dynamic_CMAC_clear(TC_AES_dynamic_CMAC* ctx)
{ if (ctx) TC_secure_zero(ctx, sizeof *ctx); }
#endif

#if defined(TC_AES_ENABLE_CMAC) && (TC_AES_ENABLE_CMAC == 1)
static void tc_aes_cmac_invalidate(struct TC_AES_CMAC_ctx* ctx)
{
  TC_AES_CMAC_ctx_clear(ctx);
  ctx->buf_len = 17; /* Reject further use until initialization succeeds. */
}

TC_status TC_AES_CMAC(const uint8_t* key, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len)
{
  struct TC_AES_CMAC_ctx ctx;
  uint8_t full[TC_AES_BLOCKLEN];
  TC_status status;

  if (key == NULL || tag == NULL ||
      tag_len < TC_AES_CMAC_MIN_TAG_LEN || tag_len > TC_AES_CMAC_TAG_MAX ||
      (msg_len != 0 && msg == NULL))
    return TC_ERROR;

  if (TC_AES_CMAC_init(&ctx, key) != TC_OK)
    return TC_ERROR;
  status = TC_AES_CMAC_update(&ctx, msg, msg_len);
  if (status == TC_OK)
    status = TC_AES_CMAC_final(&ctx, full);
  if (status == TC_OK)
    tc_aes_copy_bytes(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
  TC_AES_CMAC_ctx_clear(&ctx);
#endif
  return status;
}

TC_status TC_AES_CMAC_verify(const uint8_t* key, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_AES_CMAC_TAG_MAX];
  TC_status status;

  if (tag == NULL ||
      tag_len < TC_AES_CMAC_MIN_TAG_LEN || tag_len > TC_AES_CMAC_TAG_MAX)
    return TC_ERROR;
  if (TC_AES_CMAC(key, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  /* A mismatch is data, not malformed input. Report it distinctly. */
  status = TC_ct_equal(computed, tag, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return status;
}

/* Absorb one full block from ctx->buf into the CBC-MAC chain. */
TC_status TC_AES_CMAC_init(struct TC_AES_CMAC_ctx* ctx, const uint8_t* key)
{
  if (!ctx || TC_AES_key_init(&ctx->key, key) != TC_OK) return TC_ERROR;
  if (tc_aes_cmac_generate_subkeys(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->k1, ctx->k2) != TC_OK) {
    tc_aes_cmac_invalidate(ctx);
    return TC_ERROR;
  }
  memset(ctx->mac, 0, 16); memset(ctx->buf, 0, 16); ctx->buf_len = 0;
  return TC_OK;
}

TC_status TC_AES_CMAC_update(struct TC_AES_CMAC_ctx* ctx, const uint8_t* data, size_t length)
{
  if (!ctx || (!data && length) || ctx->buf_len > 16) return TC_ERROR;
  if (tc_aes_cmac_update(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->mac, ctx->buf,
                       &ctx->buf_len, data, length) != TC_OK) {
    tc_aes_cmac_invalidate(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_CMAC_final(struct TC_AES_CMAC_ctx* ctx, uint8_t tag[16])
{
  TC_status status;
  if (!ctx || !tag || ctx->buf_len > 16) return TC_ERROR;
  status = tc_aes_cmac_final(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->mac, ctx->buf,
                           ctx->buf_len, ctx->k1, ctx->k2, tag);
  if (status != TC_OK) {
    tc_aes_cmac_invalidate(ctx);
    return status;
  }
#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof *ctx);
#endif
  return TC_OK;
}

void TC_AES_CMAC_ctx_clear(struct TC_AES_CMAC_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

#endif /* CMAC */

#if defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1)

static TC_status tc_aes_siv_s2v(const uint8_t* k1_round, const uint8_t* const* ad,
                    const size_t* ad_lens, size_t ad_count,
                    const uint8_t* last, size_t last_len,
                    uint8_t v[TC_AES_BLOCKLEN])
{
  uint8_t d[TC_AES_BLOCKLEN];
  uint8_t tmp[TC_AES_BLOCKLEN];
  uint8_t last_block[TC_AES_BLOCKLEN];
  uint8_t k1[TC_AES_BLOCKLEN];
  uint8_t k2[TC_AES_BLOCKLEN];
  size_t i;
  const uint8_t zero[TC_AES_BLOCKLEN] = { 0 };
  TC_status status;

  /* Every S2V component uses the same CMAC key, so derive its subkeys once. */
  status = tc_aes_cmac_generate_subkeys(k1_round, TC_AES_FIXED_ROUNDS, k1, k2);
  if (status != TC_OK) goto done;
  status = tc_aes_cmac_with_subkeys(k1_round, k1, k2, zero, TC_AES_BLOCKLEN, d);
  if (status != TC_OK) goto done;
  for (i = 0; i < ad_count; ++i)
  {
    uint8_t j;
    status = tc_aes_cmac_with_subkeys(k1_round, k1, k2,
                             ad[i] != NULL ? ad[i] : zero, ad_lens[i], tmp);
    if (status != TC_OK) goto done;
    tc_aes_gf128_double(d);
    for (j = 0; j < TC_AES_BLOCKLEN; ++j)
      d[j] ^= tmp[j];
  }

  if (last_len >= TC_AES_BLOCKLEN)
  {
    /* T = last xorend D = prefix || (suffix xor D); CMAC(T). */
    tc_aes_copy_bytes(last_block, last + (last_len - TC_AES_BLOCKLEN), TC_AES_BLOCKLEN);
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      last_block[i] ^= d[i];
    status = tc_aes_cmac_concat(k1_round, k1, k2, last,
                       last_len - TC_AES_BLOCKLEN, last_block,
                       TC_AES_BLOCKLEN, v);
  }
  else
  {
    /* T = dbl(D) xor pad(last); single-block CMAC input. */
    uint8_t t[TC_AES_BLOCKLEN];
    uint8_t j;
    tc_aes_copy_bytes(t, d, TC_AES_BLOCKLEN);
    tc_aes_gf128_double(t);
    memset(tmp, 0, TC_AES_BLOCKLEN);
    if (last_len != 0 && last != NULL)
      tc_aes_copy_bytes(tmp, last, last_len);
    tmp[last_len] = 0x80;
    for (j = 0; j < TC_AES_BLOCKLEN; ++j)
      t[j] ^= tmp[j];
    status = tc_aes_cmac_with_subkeys(k1_round, k1, k2, t, TC_AES_BLOCKLEN, v);
#if TC_ZEROIZE
    TC_secure_zero(t, sizeof(t));
#endif
  }

done:
#if TC_ZEROIZE
  TC_secure_zero(d, sizeof(d));
  TC_secure_zero(tmp, sizeof(tmp));
  TC_secure_zero(last_block, sizeof(last_block));
  TC_secure_zero(k1, sizeof(k1));
  TC_secure_zero(k2, sizeof(k2));
#endif
  return status;
}

static TC_status tc_aes_siv_ctr(const uint8_t* k2_round, const uint8_t v[TC_AES_BLOCKLEN],
                    const uint8_t* input, uint8_t* output, size_t length)
{
  uint8_t counter[TC_AES_BLOCKLEN];
  uint8_t stream[TC_AES_BLOCKLEN];
  size_t offset = 0;
  uint8_t i;
  TC_status status = TC_OK;

  tc_aes_copy_bytes(counter, v, TC_AES_BLOCKLEN);
  /* Clear bit 63 and bit 31 (rightmost bit is bit 0). */
  counter[8] &= 0x7fu;
  counter[12] &= 0x7fu;

  while (offset < length)
  {
    const size_t count = length - offset < TC_AES_BLOCKLEN ?
                         length - offset : TC_AES_BLOCKLEN;
    tc_aes_copy_bytes(stream, counter, TC_AES_BLOCKLEN);
    status = tc_aes_cipher((state_t*)stream, k2_round);
    if (status != TC_OK) break;
    for (i = 0; i < (uint8_t)count; ++i)
      output[offset + i] = (uint8_t)(input[offset + i] ^ stream[i]);

    tc_internal_increment_be(counter, TC_AES_BLOCKLEN);
    offset += count;
  }

#if TC_ZEROIZE
  TC_secure_zero(counter, sizeof(counter));
  TC_secure_zero(stream, sizeof(stream));
#endif
  return status;
}

static TC_status tc_aes_siv_crypt(const uint8_t* key, const uint8_t* const* ad,
                     const size_t* ad_lens, size_t ad_count,
                     const uint8_t* input, size_t input_len,
                     uint8_t* output, uint8_t v[TC_AES_SIV_V_LEN], int decrypt)
{
  struct {
    struct TC_AES_key_ctx k1;
    struct TC_AES_key_ctx k2;
    uint8_t computed[TC_AES_BLOCKLEN];
  } st;
  size_t i;
  TC_status status = TC_ERROR;

  if (key == NULL || (ad_count != 0 && (ad == NULL || ad_lens == NULL)) ||
      ad_count > TC_AES_SIV_MAX_AD ||
      (input_len != 0 && (input == NULL || output == NULL)) || v == NULL ||
      !tc_aes_buffers_ok(input, input_len, output, input_len))
    return TC_ERROR;

  for (i = 0; i < ad_count; ++i)
  {
    if (ad_lens[i] != 0 && ad[i] == NULL)
      return TC_ERROR;
  }

  if (TC_AES_key_init(&st.k1, key) != TC_OK ||
      TC_AES_key_init(&st.k2, key + TC_AES_KEYLEN) != TC_OK)
    goto done;

  if (decrypt)
  {
    status = tc_aes_siv_ctr(st.k2.round_key, v, input, output, input_len);
    if (status == TC_OK)
      status = tc_aes_siv_s2v(st.k1.round_key, ad, ad_lens, ad_count, output, input_len,
            st.computed);
    if (status == TC_OK) status = TC_ct_equal(st.computed, v, TC_AES_BLOCKLEN);
    if (status != TC_OK)
    {
      /* SIV decrypts before authenticating. Any failure discards plaintext. */
      if (output != NULL && input_len != 0)
        TC_secure_zero(output, input_len);
    }
  }
  else
  {
    status = tc_aes_siv_s2v(st.k1.round_key, ad, ad_lens, ad_count, input, input_len, v);
    if (status == TC_OK) {
      status = tc_aes_siv_ctr(st.k2.round_key, v, input, output, input_len);
      if (status != TC_OK && output != NULL && input_len != 0)
        TC_secure_zero(output, input_len);
    }
  }

done:
#if TC_ZEROIZE
  TC_secure_zero(&st, sizeof(st));
#endif
  return status;
}

TC_status TC_AES_SIV_encrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t v[TC_AES_SIV_V_LEN],
                    uint8_t* ciphertext)
{
  uint8_t local_v[TC_AES_SIV_V_LEN];
  int status;

  if (v == NULL)
    return TC_ERROR;
  /*
   * V is written after ciphertext. If they overlap, the post-encrypt copy
   * would clobber ciphertext (exact or partial). Stage V for pt alias only.
   */
  if (!tc_aes_buffers_disjoint(v, TC_AES_SIV_V_LEN, ciphertext, plaintext_len))
    return TC_ERROR;
  status = tc_aes_siv_crypt(key, ad, ad_lens, ad_count, plaintext, plaintext_len,
                     ciphertext, local_v, 0);
  if (status == TC_OK)
    tc_aes_copy_bytes(v, local_v, TC_AES_SIV_V_LEN);
#if TC_ZEROIZE
  TC_secure_zero(local_v, sizeof(local_v));
#endif
  return status;
}

TC_status TC_AES_SIV_decrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t v[TC_AES_SIV_V_LEN],
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    uint8_t* plaintext)
{
  uint8_t local_v[TC_AES_SIV_V_LEN];
  int status;

  if (v == NULL)
    return TC_ERROR;
  if (!tc_aes_buffers_disjoint(v, TC_AES_SIV_V_LEN,
                               plaintext, ciphertext_len))
    return TC_ERROR;
  tc_aes_copy_bytes(local_v, v, TC_AES_SIV_V_LEN);
  status = tc_aes_siv_crypt(key, ad, ad_lens, ad_count, ciphertext, ciphertext_len,
                     plaintext, local_v, 1);
#if TC_ZEROIZE
  TC_secure_zero(local_v, sizeof(local_v));
#endif
  return status;
}

#endif /* SIV */

#endif /* CMAC || SIV */
