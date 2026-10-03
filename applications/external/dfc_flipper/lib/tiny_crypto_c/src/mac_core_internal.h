/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_MAC_CORE_INTERNAL_H_
#define TC_MAC_CORE_INTERNAL_H_
#include "block_cipher_internal.h"

/* Big-endian GF(2^n) doubling (SP 800-38B). input and output may be the
 * same block. */
void tc_mac_gf_double(uint8_t* output, const uint8_t* input, size_t block_size, uint8_t reduction);
/* Byte-reversed doubling used by EAX' (ANSI C12.22): shifts toward higher
 * indexes and reduces into byte zero. */
void tc_mac_gf_double_reversed(uint8_t* output, const uint8_t* input, size_t block_size,
                               uint8_t reduction);

/* SP 800-38B subkeys: L = E_K(0), K1 = dbl(L), K2 = dbl(K1). reversed selects
 * tc_mac_gf_double_reversed. Failure wipes k1 and k2. */
TC_status tc_mac_derive_subkeys(const tc_block_cipher* cipher, uint8_t reduction, int reversed,
                                uint8_t* k1, uint8_t* k2);

/* The caller owns the chaining value and partial block. used is at most
 * block_size. retain_last keeps a complete final block for CMAC subkey
 * selection. */
TC_status tc_mac_cbc_block(const tc_block_cipher* cipher, uint8_t* mac, const uint8_t* block);
TC_status tc_mac_cbc_update(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block,
                            uint8_t* used, const uint8_t* data, size_t length, int retain_last);
TC_status tc_mac_cbc_pad(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block,
                         uint8_t* used);
TC_status tc_mac_cmac_final(const tc_block_cipher* cipher, uint8_t* mac, uint8_t* block,
                            uint8_t used, const uint8_t* complete_subkey,
                            const uint8_t* partial_subkey, uint8_t* tag);

/* CMAC of the concatenated parts. The CBC chain starts from initial, which is
 * zero for SP 800-38B CMAC; EAX' starts from D or Q. Working state is local
 * and wiped. */
TC_status tc_mac_cmac_parts(const tc_block_cipher* cipher, const uint8_t* initial,
                            const TC_bytes* parts, size_t count, const uint8_t* complete_subkey,
                            const uint8_t* partial_subkey, uint8_t* tag);
#endif
