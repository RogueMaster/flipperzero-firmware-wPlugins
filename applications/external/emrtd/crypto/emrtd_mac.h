/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Message authentication and padding for Secure Messaging.
 * ICAO Doc 9303 part 11, section 9.8.
 *
 * Two families are in use:
 *   - 3DES: Retail MAC, ISO/IEC 9797-1 algorithm 3 with DES and output
 *     transformation 3, truncated to eight bytes (BAC).
 *   - AES: CMAC per NIST SP 800-38B, eight bytes of the sixteen byte result
 *     (PACE and Chip Authentication).
 */
#pragma once

#include "emrtd_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Append ISO/IEC 9797-1 padding method 2 in place.
 *
 * One 0x80 byte followed by zeroes up to a multiple of @p block. The padding
 * is always added, even when the input already aligns.
 *
 * @param[in,out] buf    buffer holding @p len bytes, with room for the padding
 * @param[in]     len    current length
 * @param[in]     block  block size, 8 or 16
 * @return the padded length
 */
size_t emrtd_pad_iso9797_m2(uint8_t* buf, size_t len, size_t block);

/** Length a buffer of @p len bytes grows to once method 2 padding is added. */
size_t emrtd_padded_len(size_t len, size_t block);

/**
 * Strip ISO/IEC 9797-1 padding method 2.
 *
 * @return false when the trailing 0x80 marker is missing, which means the
 *         plaintext is not what we think it is.
 */
bool emrtd_unpad_iso9797_m2(const uint8_t* data, size_t len, size_t* out_len);

/**
 * Retail MAC (ISO/IEC 9797-1 algorithm 3) over already padded data.
 *
 * @param[in]  key   K1 || K2, sixteen bytes
 * @param[in]  data  input, length must be a multiple of eight
 * @param[out] mac   eight byte result
 * @return false if @p len is not a multiple of eight
 */
bool emrtd_retail_mac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t mac[8]);

/**
 * AES-CMAC (NIST SP 800-38B), truncated to @p mac_len bytes.
 *
 * Unlike the Retail MAC this one pads internally per the CMAC specification,
 * so @p data is passed exactly as it is to be authenticated.
 *
 * @param[in]  key      AES key
 * @param[in]  key_len  16, 24 or 32
 * @param[out] mac      result, @p mac_len bytes
 */
bool emrtd_aes_cmac(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t len,
    uint8_t* mac,
    size_t mac_len);

#ifdef __cplusplus
}
#endif
