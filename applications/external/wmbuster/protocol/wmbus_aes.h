#pragma once
/*
 * AES-128 decryption helpers for wM-Bus / OMS encryption modes.
 *
 *   Mode 5: AES-128-CBC, IV = M(2) || A(8) || AccessNo repeated to 16 bytes
 *   Mode 7: AES-128-CTR, IV = M(2) || A(8) || AccessNo || counter
 *
 * Backed by Flipper's `furi_hal_crypto` HW accelerator. A pure-C software
 * fallback (a small AES-128 implementation) is compiled in for off-device
 * unit tests via -DWMBUS_AES_SOFT.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint16_t m_field;
    uint8_t  a_field[8];   /* ID(4) Ver(1) Type(1) AccessNo(1) reserved(1) */
    uint8_t  access_no;
} WmbusCryptoIvCtx;

/* Build the 16-byte IV for Mode 5 (CBC). */
void wmbus_aes_build_iv5(const WmbusCryptoIvCtx* ctx, uint8_t iv[16]);

/* Decrypt `in` of length `len` (must be multiple of 16) into `out`. */
bool wmbus_aes_cbc_decrypt(
    const uint8_t key[16], const uint8_t iv[16],
    const uint8_t* in, size_t len, uint8_t* out);

/* Verify the OMS plaintext prefix `0x2F 0x2F` to detect a successful
 * decryption. */
bool wmbus_aes_verify_2f2f(const uint8_t* plain, size_t len);
