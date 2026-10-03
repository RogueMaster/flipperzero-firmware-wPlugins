/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Block-cipher MAC building blocks shared by AES and DES: CBC-MAC chaining,
 * CMAC subkeys and finalization (SP 800-38B), and GF doubling. */
#include "mac_core_internal.h"
#include <string.h>

void tc_mac_gf_double(uint8_t* output, const uint8_t* input, size_t block_size, uint8_t reduction)
{
  uint8_t carry = 0;
  for (size_t i = block_size; i > 0; --i) {
    const size_t offset = i - 1;
    const uint8_t next = (uint8_t)(input[offset] >> 7);
    output[offset] = (uint8_t)((input[offset] << 1) | carry);
    carry = next;
  }
  output[block_size - 1] ^= (uint8_t)(reduction & (uint8_t)(0u - carry));
}

void tc_mac_gf_double_reversed(uint8_t* output, const uint8_t* input, size_t block_size,
                               uint8_t reduction)
{
  uint8_t carry = 0;
  for (size_t i = 0; i < block_size; ++i) {
    const uint8_t next = (uint8_t)(input[i] >> 7);
    output[i] = (uint8_t)((input[i] << 1) | carry);
    carry = next;
  }
  output[0] ^= (uint8_t)(reduction & (uint8_t)(0u - carry));
}

TC_status tc_mac_derive_subkeys(const tc_block_cipher* cipher, uint8_t reduction, int reversed,
                                uint8_t* k1, uint8_t* k2)
{
  const size_t width = cipher->block_size;
  uint8_t l[TC_BLOCK_MAX] = {0};
  TC_status status = cipher->encrypt(cipher->key, l);
  if (status == TC_OK) {
    if (reversed) {
      tc_mac_gf_double_reversed(k1, l, width, reduction);
      tc_mac_gf_double_reversed(k2, k1, width, reduction);
    } else {
      tc_mac_gf_double(k1, l, width, reduction);
      tc_mac_gf_double(k2, k1, width, reduction);
    }
  } else {
    TC_secure_zero(k1, width);
    TC_secure_zero(k2, width);
  }
  TC_secure_zero(l, sizeof l);
  return status;
}

TC_status tc_mac_cbc_block(const tc_block_cipher* cipher, uint8_t* mac, const uint8_t* block)
{
  for (size_t i = 0; i < cipher->block_size; ++i)
    mac[i] ^= block[i];
  return cipher->encrypt(cipher->key, mac);
}

TC_status tc_mac_cbc_update(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block,
                            uint8_t* used, const uint8_t* data, size_t length, int retain_last)
{
  const size_t width = cipher->block_size;
  while (length) {
    if (*used == width) {
      if (tc_mac_cbc_block(cipher, mac, block) != TC_OK)
        return TC_ERROR;
      *used = 0;
    }
    if (!*used && (retain_last ? length > width : length >= width)) {
      if (tc_mac_cbc_block(cipher, mac, data) != TC_OK)
        return TC_ERROR;
      data += width;
      length -= width;
      continue;
    }
    size_t take = width - *used;
    if (take > length)
      take = length;
    memcpy(block + *used, data, take);
    *used = (uint8_t)(*used + take);
    data += take;
    length -= take;
  }
  if (!retain_last && *used == width) {
    if (tc_mac_cbc_block(cipher, mac, block) != TC_OK)
      return TC_ERROR;
    *used = 0;
  }
  return TC_OK;
}

TC_status tc_mac_cbc_pad(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block, uint8_t* used)
{
  if (*used) {
    memset(block + *used, 0, cipher->block_size - *used);
    if (tc_mac_cbc_block(cipher, mac, block) != TC_OK)
      return TC_ERROR;
    *used = 0;
    memset(block, 0, cipher->block_size);
  }
  return TC_OK;
}

TC_status tc_mac_cmac_final(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block,
                            uint8_t used, const uint8_t* complete_subkey,
                            const uint8_t* partial_subkey, uint8_t* tag)
{
  const size_t width = cipher->block_size;
  if (used != width) {
    memset(block + used, 0, width - used);
    block[used] = 0x80;
  }
  const uint8_t* subkey = used == width ? complete_subkey : partial_subkey;
  for (size_t i = 0; i < width; ++i)
    block[i] ^= subkey[i];
  if (tc_mac_cbc_block(cipher, mac, block) != TC_OK)
    return TC_ERROR;
  memcpy(tag, mac, width);
  return TC_OK;
}

TC_status tc_mac_cmac_parts(const tc_block_cipher* cipher, const uint8_t* initial,
                            const TC_bytes* parts, size_t count, const uint8_t* complete_subkey,
                            const uint8_t* partial_subkey, uint8_t* tag)
{
  uint8_t mac[TC_BLOCK_MAX] = {0}, block[TC_BLOCK_MAX] = {0};
  uint8_t used = 0;
  TC_status status = TC_OK;
  if (initial)
    memcpy(mac, initial, cipher->block_size);
  for (size_t i = 0; status == TC_OK && i < count; ++i)
    status = tc_mac_cbc_update(cipher, mac, block, &used, parts[i].data, parts[i].length, 1);
  if (status == TC_OK)
    status = tc_mac_cmac_final(cipher, mac, block, used, complete_subkey, partial_subkey, tag);
  TC_secure_zero(mac, sizeof mac);
  TC_secure_zero(block, sizeof block);
  return status;
}
