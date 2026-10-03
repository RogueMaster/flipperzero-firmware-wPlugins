/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* SSKDF wrappers for sskdf.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_SSKDF_HPP_
#define TINY_CRYPTO_SSKDF_HPP_

#ifndef __cplusplus
#error Do not include sskdf.hpp in a C project, include sskdf.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/sskdf.h>

#if TC_ENABLE_SSKDF
namespace tiny_crypto {

/* One wrapper per enabled SHA. Status values and contracts come from sskdf.h.
 * The macro is file-local and undefined at the end of this header. */
#define TINY_CRYPTO_SSKDF_FAMILY(cpp_name, C_NAME)                                                 \
  TC_CPP_NODISCARD inline TC_status cpp_name(bytes z, const bytes* info, size_t count,             \
                                             buffer output) noexcept                               \
  {                                                                                                \
    return ::TC_SSKDF_##C_NAME(z, info, count, output);                                            \
  }

#if TC_ENABLE_SHA1
TINY_CRYPTO_SSKDF_FAMILY(sskdf_sha1, SHA1)
#endif
#if TC_ENABLE_SHA224
TINY_CRYPTO_SSKDF_FAMILY(sskdf_sha224, SHA224)
#endif
#if TC_ENABLE_SHA256
TINY_CRYPTO_SSKDF_FAMILY(sskdf_sha256, SHA256)
#endif
#if TC_ENABLE_SHA384
TINY_CRYPTO_SSKDF_FAMILY(sskdf_sha384, SHA384)
#endif
#if TC_ENABLE_SHA512
TINY_CRYPTO_SSKDF_FAMILY(sskdf_sha512, SHA512)
#endif

#undef TINY_CRYPTO_SSKDF_FAMILY

} // namespace tiny_crypto
#endif /* TC_ENABLE_SSKDF */
#endif
