/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * DES block cipher and key schedule shared by the modes (des_modes.c) and
 * MACs (des_mac.c). */
#ifndef TC_DES_INTERNAL_H_
#define TC_DES_INTERNAL_H_

#include <tiny_crypto/des.h>
#include "block_cipher_internal.h"

/* Reject weak and semi-weak component keys and bundles that collapse to
 * single DES. keylen is 8, 16 or 24. */
int tc_des_bundle_is_rejected(const uint8_t* key, size_t keylen);
/* Compare effective DES keys while ignoring parity bits. */
int tc_des_keys_equal(const uint8_t* left, const uint8_t* right);
/* Expand one 8-byte key into 16 round subkeys of 6 bytes. */
void tc_des_key_schedule(uint8_t (*sk)[6], const uint8_t* key);
/* Expand an 8-byte key into 16 subkeys, or a 16- or 24-byte TDEA bundle into
 * 48 subkeys (K1, K2, K3). keylen is 8, 16 or 24 and sk holds enough rows. */
void tc_des_schedule_key(uint8_t (*sk)[6], const uint8_t* key, size_t keylen);
/* Encrypt or decrypt one block with a 16-subkey schedule. */
void tc_des_cipher_block(const uint8_t (*sk)[6], uint8_t* buf, int decrypt);
/* One DES stage, or the TDEA encrypt-decrypt-encrypt bundle when triple. */
void tc_des_encrypt_scheduled(const void* schedule, uint8_t block[TC_DES_BLOCKLEN], int triple);
/* Inverse of tc_des_encrypt_scheduled: D(K3), E(K2), D(K1) when triple. */
void tc_des_decrypt_scheduled(const void* schedule, uint8_t block[TC_DES_BLOCKLEN], int triple);

/* A DES schedule, or a TDEA bundle when triple, borrowed by a descriptor. */
typedef struct {
  const void* schedule;
  int triple;
} tc_des_block_key;

/* The DES block cipher cannot fail. */
static inline TC_status tc_des_block_encrypt(const void* key, uint8_t* block)
{
  const tc_des_block_key* bundle = (const tc_des_block_key*)key;
  tc_des_encrypt_scheduled(bundle->schedule, block, bundle->triple);
  return TC_OK;
}

static inline TC_status tc_des_block_decrypt(const void* key, uint8_t* block)
{
  const tc_des_block_key* bundle = (const tc_des_block_key*)key;
  tc_des_decrypt_scheduled(bundle->schedule, block, bundle->triple);
  return TC_OK;
}

#if defined(TC_TEST_DES_FAULT)
/* Test seam: the test supplies a forward cipher that fails on request, so the
 * failure paths shared with fallible AES backends can run with DES. */
TC_status tc_test_des_block_encrypt(const void* key, uint8_t* block);
#define TC_DES_DESCRIPTOR_ENCRYPT tc_test_des_block_encrypt
#else
#define TC_DES_DESCRIPTOR_ENCRYPT tc_des_block_encrypt
#endif

/* Forward-only descriptor for the stream modes, CBC encryption and the MACs. */
static inline tc_block_cipher tc_des_block_cipher(const tc_des_block_key* key)
{
  const tc_block_cipher cipher = {TC_DES_BLOCKLEN, key, TC_DES_DESCRIPTOR_ENCRYPT, NULL};
  return cipher;
}

/* Descriptor with the inverse cipher, for CBC decryption. */
static inline tc_block_cipher tc_des_block_cipher_inverse(const tc_des_block_key* key)
{
  const tc_block_cipher cipher = {TC_DES_BLOCKLEN, key, TC_DES_DESCRIPTOR_ENCRYPT,
                                  tc_des_block_decrypt};
  return cipher;
}

#endif
