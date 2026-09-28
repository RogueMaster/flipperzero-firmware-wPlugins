/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_AES_PLATFORM_INTERNAL_H
#define TC_AES_PLATFORM_INTERNAL_H

#include <tiny_crypto/common.h>

#ifndef TC_AES_PLATFORM
#define TC_AES_PLATFORM 0
#endif

typedef enum {
  TC_AES_PLATFORM_OK,
  TC_AES_PLATFORM_UNSUPPORTED,
  TC_AES_PLATFORM_ERROR
} tc_aes_platform_result;

/* Key is the original 16/24/32-byte key, not a transformed decryption schedule.
 * UNSUPPORTED leaves the block unchanged; ERROR must not trigger fallback. */
tc_aes_platform_result tc_aes_platform_block(const uint8_t* key, uint8_t rounds,
                                            uint8_t block[16], int decrypt);

#endif
