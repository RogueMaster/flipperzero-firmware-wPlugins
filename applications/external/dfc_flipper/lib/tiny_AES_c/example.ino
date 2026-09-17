/*
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 *
 * Simple embedded application example for tiny-AES-c
 */
#include "aes.h"
#include <stdint.h>

void setup(void)
{
    uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    uint8_t iv[16]  = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t data[16] = {'H', 'e', 'l', 'l', 'o', ' ', '1', '6',
                        'B', ' ', 'W', 'o', 'r', 'l', 'd', '!'};

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_crypt(&ctx, data, sizeof(data));
}

void loop(void)
{
}
