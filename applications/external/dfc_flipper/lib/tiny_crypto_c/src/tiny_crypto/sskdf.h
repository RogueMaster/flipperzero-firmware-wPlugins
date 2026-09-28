/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SSKDF_H_
#define TINY_CRYPTO_SSKDF_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Hash-based single-step KDF: H(counter32be || Z || OtherInfo).
 * Z and output must be nonempty. OtherInfo is the concatenation of the spans.
 * Output must not overlap inputs or the span array. Invalid arguments leave
 * output unchanged; a hash failure wipes output. All lengths are bytes. */
#if TC_ENABLE_SHA256
TC_status TC_SSKDF_SHA256(const uint8_t* z, size_t z_len,
    const TC_bytes* info, size_t count, uint8_t* output, size_t output_len);
#endif
#if TC_ENABLE_SHA384
TC_status TC_SSKDF_SHA384(const uint8_t* z, size_t z_len,
    const TC_bytes* info, size_t count, uint8_t* output, size_t output_len);
#endif
#ifdef __cplusplus
}
#endif
#endif
