/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_COMMON_HPP_
#define TINY_CRYPTO_COMMON_HPP_

#ifndef __cplusplus
#error Do not include common.hpp in a C project, include common.h instead
#endif

#include <tiny_crypto/common.h>

namespace tiny_crypto {

typedef ::TC_bytes bytes;
typedef ::TC_credential_status credential_status;

/* The byte count is public. This only avoids content-dependent early exit. */
inline TC_status ct_equal(const uint8_t* a, const uint8_t* b,
                          size_t length) noexcept {
    return ::TC_ct_equal(a, b, length);
}

}  // namespace tiny_crypto

#endif
