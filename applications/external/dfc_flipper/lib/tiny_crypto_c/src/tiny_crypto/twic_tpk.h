/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* TWIC Privacy Key (TPK) containers from the card or barcode, and
 * TWIC private-object decryption.
 * Standards: TWIC Part 2 v5 sections 4.6.2 and 4.9.
 * Configuration: TC_ENABLE_TWIC_TPK and TC_ENABLE_TWIC_OBJECT_CRYPTO.
 * Contracts: docs/api.md. Guide: docs/twic-barcode.md. */
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
typedef struct {
  uint8_t key[TC_TWIC_TPK_BYTES];
} TC_TWIC_tpk;
#if TC_ENABLE_TWIC_TPK
typedef enum {
  TC_TWIC_TPK_CARD,
  TC_TWIC_TPK_BARCODE_HEX,
  TC_TWIC_TPK_CONTENTS
} TC_TWIC_tpk_encoding;

/* Read the TWIC Privacy Key from a complete DFC101 container, its
 * hexadecimal ZTA barcode field or its C0/C1/C2 contents (TWIC Part 2 v5
 * sections 4.6.2 and 4.9). CONTENTS starts at the C0 field after application
 * framing has been removed. Barcode mode also accepts the transposed DCF101
 * prefix of the printed example in section 4.9. Requires AES-128 algorithm
 * identifier 08 and reserved key index 00. out must be disjoint from input.
 * The decoded barcode copy lives on the stack and is wiped on return. Wipe
 * out with TC_secure_zero after use. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, an unknown encoding or overlap. LIMIT for a card container or
 * contents above 44 bytes. INVALID for bad framing, hex digits, field order
 * or sizes, or a nonzero key index. UNSUPPORTED for another algorithm or a
 * key without 16 bytes. out changes only on OK. */
TC_TLV_result TC_TWIC_tpk_read(TC_bytes input, TC_TWIC_tpk_encoding encoding, TC_TWIC_tpk* out);
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO
/* Encrypt a TWIC private object in place with AES-128 ECB under the TPK and
 * PKCS#7 padding (TWIC Part 2 v5 sections 4.4.1 and 4.6.4). buffer.capacity
 * must hold plaintext_length plus its padding, so a
 * block-aligned object grows by a full block. key, the buffer capacity and
 * ciphertext_length must be disjoint. Bytes beyond the padded region are
 * never written. Runtime S-box builds require TC_AES_init_sbox() during
 * application startup.
 * Returns TC_OK with ciphertext_length written. TC_ERROR for NULL
 * arguments, a short buffer, a size overflow or overlap, with all buffers
 * unchanged. A cipher failure returns TC_ERROR, wipes the padded region and
 * leaves ciphertext_length unchanged. */
TC_status TC_TWIC_object_encrypt(const TC_TWIC_tpk* key, TC_buffer buffer, size_t plaintext_length,
                                 size_t* ciphertext_length);
/* Decrypt a complete enciphered BC value in place and check its PKCS#7
 * padding in constant time (TWIC Part 2 v5 sections 4.4.1 and 4.6.4). On TC_OK,
 * plaintext_length excludes the padding and the removed padding bytes are
 * wiped. key, buffer and plaintext_length must be disjoint. Authenticate the
 * recovered object's signature before using its contents. ciphertext.capacity
 * is the complete encrypted length.
 * Returns TC_OK with plaintext_length written. TC_ERROR for NULL arguments,
 * an empty length or one outside the multiples of 16, or overlap, with all
 * buffers unchanged. Bad padding or a cipher failure returns TC_ERROR, wipes
 * the entire buffer and leaves plaintext_length unchanged. */
TC_status TC_TWIC_object_decrypt(const TC_TWIC_tpk* key, TC_buffer ciphertext,
                                 size_t* plaintext_length);
#endif
#ifdef __cplusplus
}
#endif
#endif
