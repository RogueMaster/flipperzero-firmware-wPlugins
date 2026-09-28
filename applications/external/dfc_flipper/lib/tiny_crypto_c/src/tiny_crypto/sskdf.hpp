/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SSKDF_HPP_
#define TINY_CRYPTO_SSKDF_HPP_
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/sskdf.h>

namespace tiny_crypto {
#if TC_ENABLE_SHA256
inline TC_status sskdf_sha256(bytes z, const bytes* info, size_t count,
                              uint8_t* output, size_t length) noexcept {
    return ::TC_SSKDF_SHA256(z.data, z.length, info, count, output, length);
}
#endif
#if TC_ENABLE_SHA384
inline TC_status sskdf_sha384(bytes z, const bytes* info, size_t count,
                              uint8_t* output, size_t length) noexcept {
    return ::TC_SSKDF_SHA384(z.data, z.length, info, count, output, length);
}
#endif
} // namespace tiny_crypto
#endif
