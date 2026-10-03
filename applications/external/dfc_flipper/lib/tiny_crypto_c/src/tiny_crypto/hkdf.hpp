/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* HKDF wrappers for hkdf.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_HKDF_HPP_
#define TINY_CRYPTO_HKDF_HPP_

#ifndef __cplusplus
#error Do not include hkdf.hpp in a C project, include hkdf.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/hkdf.h>

namespace tiny_crypto {

#if TC_ENABLE_HKDF
/* RFC 5869 HKDF. Inputs are borrowed byte spans, and the input keying
 * material is the concatenation of ikm_count spans. extract writes one
 * digest into a digest-sized array. The C functions document the argument
 * and failure rules. */
#define TINY_CRYPTO_HKDF_FAMILY(N)                                                                 \
  TC_CPP_NODISCARD inline TC_status hkdf_sha##N##_extract(                                         \
      bytes salt, const bytes* ikm, size_t ikm_count,                                              \
      uint8_t (&prk)[TC_SHA##N##_DIGESTLEN]) noexcept                                              \
  {                                                                                                \
    return ::TC_HKDF_SHA##N##_extract(salt, ikm, ikm_count, prk);                                  \
  }                                                                                                \
  TC_CPP_NODISCARD inline TC_status hkdf_sha##N##_expand(bytes prk, bytes info,                    \
                                                         buffer output) noexcept                   \
  {                                                                                                \
    return ::TC_HKDF_SHA##N##_expand(prk, info, output);                                           \
  }                                                                                                \
  TC_CPP_NODISCARD inline TC_status hkdf_sha##N##_derive(                                          \
      bytes salt, const bytes* ikm, size_t ikm_count, bytes info, buffer output) noexcept          \
  {                                                                                                \
    return ::TC_HKDF_SHA##N##_derive(salt, ikm, ikm_count, info, output);                          \
  }                                                                                                \
  TC_CPP_NODISCARD inline TC_status hkdf_sha##N##_derive(bytes salt, bytes ikm, bytes info,        \
                                                         buffer output) noexcept                   \
  {                                                                                                \
    return hkdf_sha##N##_derive(salt, &ikm, 1, info, output);                                      \
  }

#if TC_ENABLE_SHA1
TINY_CRYPTO_HKDF_FAMILY(1)
#endif
#if TC_ENABLE_SHA224
TINY_CRYPTO_HKDF_FAMILY(224)
#endif
#if TC_ENABLE_SHA256
TINY_CRYPTO_HKDF_FAMILY(256)
#endif
#if TC_ENABLE_SHA384
TINY_CRYPTO_HKDF_FAMILY(384)
#endif
#if TC_ENABLE_SHA512
TINY_CRYPTO_HKDF_FAMILY(512)
#endif

#undef TINY_CRYPTO_HKDF_FAMILY
#endif /* TC_ENABLE_HKDF */

} // namespace tiny_crypto

#endif /* TINY_CRYPTO_HKDF_HPP_ */
