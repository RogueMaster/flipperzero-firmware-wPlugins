/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_TWIC_TPK_H_
#define TINY_CRYPTO_TWIC_TPK_H_
#include <tiny_crypto/common.h>
#if TC_ENABLE_TWIC_TPK
#include <tiny_crypto/tlv.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum { TC_TWIC_TPK_BYTES = 16 };
typedef struct { uint8_t key[TC_TWIC_TPK_BYTES]; } TC_TWIC_tpk;
#if TC_ENABLE_TWIC_TPK
typedef enum {
  TC_TWIC_TPK_CARD, TC_TWIC_TPK_BARCODE_HEX, TC_TWIC_TPK_CONTENTS
} TC_TWIC_tpk_encoding;

/* Read a complete DFC101 container or its hexadecimal ZTA field value.
 * CONTENTS starts at the C0 field after application framing has been removed.
 * Barcode mode also accepts the DCF101 prefix in TWIC Part 2 section 4.9.
 * Requires AES-128 algorithm identifier 08 and reserved key index 00.
 * out changes only on OK and must be disjoint from input. Wipe out with
 * TC_secure_zero after use. Requires TLV support. */
TC_TLV_result TC_TWIC_tpk_read(TC_bytes input, TC_TWIC_tpk_encoding encoding, TC_TWIC_tpk* out);
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO
/* Runtime S-box builds require TC_AES_init_sbox() during application startup. */
/* Encrypt an object in place with PKCS#7 padding. Capacity must accommodate
 * length + (TC_AES_BLOCKLEN - length % TC_AES_BLOCKLEN) bytes.
 * Bad arguments preserve buffers. Processing failures wipe the padded region
 * and preserve ciphertext_length. Key, buffer capacity and output-length
 * storage must be disjoint. Bytes beyond the padded region remain unchanged. */
TC_status TC_TWIC_object_encrypt(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, size_t capacity, size_t* ciphertext_length);
/* Decrypt a complete enciphered BC value in place and check PKCS#7 padding.
 * On OK, plaintext_length excludes padding; removed padding bytes are wiped.
 * Bad arguments preserve buffers. Processing failures wipe the entire buffer
 * and preserve plaintext_length. All buffers and key storage must be disjoint.
 * Authenticate the recovered object's signature before using its contents. */
TC_status TC_TWIC_object_decrypt(const TC_TWIC_tpk* key, uint8_t* buffer,
    size_t length, size_t* plaintext_length);
#endif
#ifdef __cplusplus
}
#endif
#endif
