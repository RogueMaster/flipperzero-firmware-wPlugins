/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/twic_tpk.h>
#if TC_ENABLE_TWIC_OBJECT_CRYPTO
#include <tiny_crypto/aes.h>
#include "internal.h"

static int storage_valid(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, size_t* output_length)
{
  return key && buffer && output_length &&
      tc_internal_ranges_disjoint(buffer,length,key,sizeof *key) &&
      tc_internal_ranges_disjoint(buffer,length,output_length,sizeof *output_length) &&
      tc_internal_ranges_disjoint(key,sizeof *key,output_length,sizeof *output_length);
}

static TC_status transform(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, int encrypt)
{
  struct TC_AES_key_ctx aes;
  TC_status result = TC_AES_key_init(&aes,key->key);
  for (size_t offset = 0; result == TC_OK && offset < length; offset += TC_AES_BLOCKLEN)
    result = encrypt ? TC_AES_ECB_encrypt(&aes,buffer + offset) :
        TC_AES_ECB_decrypt(&aes,buffer + offset);
  TC_secure_zero(&aes,sizeof aes);
  if (result != TC_OK) TC_secure_zero(buffer,length);
  return result;
}

TC_status TC_TWIC_object_encrypt(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, size_t capacity, size_t* ciphertext_length)
{
  const size_t padding = TC_AES_BLOCKLEN - length % TC_AES_BLOCKLEN;
  /* A block-aligned plaintext still needs a complete padding block. */
  if (length > SIZE_MAX - padding || capacity < length + padding ||
      !storage_valid(key,buffer,capacity,ciphertext_length)) return TC_ERROR;
  memset(buffer + length,(int)padding,padding);
  if (transform(key,buffer,length + padding,1) != TC_OK) return TC_ERROR;
  *ciphertext_length = length + padding;
  return TC_OK;
}

TC_status TC_TWIC_object_decrypt(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, size_t* plaintext_length)
{
  TC_status result = TC_ERROR;
  if (!length || length % TC_AES_BLOCKLEN ||
      !storage_valid(key,buffer,length,plaintext_length)) return TC_ERROR;
  if (transform(key,buffer,length,0) != TC_OK) return TC_ERROR;
  const unsigned padding = buffer[length - 1];
  unsigned invalid = (padding == 0) | (padding > TC_AES_BLOCKLEN);
  /* Check the complete final block before deciding whether padding is valid. */
  for (unsigned i = 1; i <= TC_AES_BLOCKLEN; ++i)
    invalid |= (buffer[length - i] ^ padding) & (0u - (unsigned)(i <= padding));
  if (invalid) goto cleanup;
  TC_secure_zero(buffer + length - padding,padding);
  *plaintext_length = length - padding;
  result = TC_OK;
cleanup:
  if (result != TC_OK) TC_secure_zero(buffer,length);
  return result;
}
#endif
