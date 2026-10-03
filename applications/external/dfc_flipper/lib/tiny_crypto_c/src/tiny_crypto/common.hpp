/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* Shared C++ types: bytes, buffer, ct_equal and TC_CPP_NODISCARD.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_COMMON_HPP_
#define TINY_CRYPTO_COMMON_HPP_

#ifndef __cplusplus
#error Do not include common.hpp in a C project, include common.h instead
#endif

#include <tiny_crypto/common.h>

#if __cplusplus >= 201703L
#define TC_CPP_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#define TC_CPP_NODISCARD __attribute__((warn_unused_result))
#else
#define TC_CPP_NODISCARD
#endif

namespace tiny_crypto {

typedef ::TC_bytes bytes;
typedef ::TC_buffer buffer;
typedef ::TC_result result;
typedef ::TC_status status;
typedef ::TC_credential_status credential_status;

/* Compare two byte spans. Returns TC_OK when both hold the same bytes and
 * TC_MISMATCH when the contents or the lengths differ. A span with NULL data
 * and a nonzero length returns TC_ERROR. Lengths are public. The scan covers
 * the common length and its timing is independent of content. */
TC_CPP_NODISCARD inline TC_status ct_equal(bytes a, bytes b) noexcept
{
  return ::TC_ct_equal(a, b);
}

} // namespace tiny_crypto

#endif
