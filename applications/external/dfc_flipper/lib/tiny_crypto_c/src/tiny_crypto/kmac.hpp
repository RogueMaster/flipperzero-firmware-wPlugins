/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The KMAC256 class for kmac.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_KMAC_HPP_
#define TINY_CRYPTO_KMAC_HPP_
#ifndef __cplusplus
#error Do not include kmac.hpp in a C project, include kmac.h instead
#endif
#include <tiny_crypto/kmac.h>
#include <tiny_crypto/common.hpp>
#if TC_ENABLE_KMAC256
namespace tiny_crypto {
/* Streaming KMAC256. init takes a key and an optional customization string.
 * finish writes out.capacity bytes, a length that is part of the MAC input,
 * and consumes the key. finish and clear leave the object unkeyed, and update
 * and finish then return TC_ERROR until the next successful init. A rejected
 * init wipes the prior keyed state. The destructor clears the context. */
class KMAC256 {
  TC_KMAC256_ctx ctx_{};

public:
  KMAC256() noexcept = default;
  KMAC256(const KMAC256&) = delete;
  KMAC256& operator=(const KMAC256&) = delete;
  ~KMAC256() noexcept
  {
    clear();
  }
  /* The C functions in kmac.h document the span, length and failure rules. */
  TC_CPP_NODISCARD TC_status init(bytes key, bytes custom = bytes{nullptr, 0}) noexcept
  {
    return TC_KMAC256_init(&ctx_, key, custom);
  }
  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return TC_KMAC256_update(&ctx_, data);
  }
  TC_CPP_NODISCARD TC_status finish(buffer out) noexcept
  {
    return TC_KMAC256_final(&ctx_, out);
  }
  TC_CPP_NODISCARD TC_status finish_short_tag(buffer out) noexcept
  {
    return TC_KMAC256_final_short_tag(&ctx_, out);
  }
  void clear() noexcept
  {
    TC_KMAC256_ctx_clear(&ctx_);
  }
  TC_CPP_NODISCARD static TC_status digest(bytes key, bytes data, bytes custom, buffer out) noexcept
  {
    return TC_KMAC256_digest(key, data, custom, out);
  }
  TC_CPP_NODISCARD static TC_status digest_short_tag(bytes key, bytes data, bytes custom,
                                                     buffer out) noexcept
  {
    return TC_KMAC256_digest_short_tag(key, data, custom, out);
  }
  TC_CPP_NODISCARD static TC_status verify(bytes key, bytes data, bytes custom, bytes tag) noexcept
  {
    return TC_KMAC256_verify(key, data, custom, tag);
  }
  TC_CPP_NODISCARD static TC_status verify_short_tag(bytes key, bytes data, bytes custom,
                                                     bytes tag) noexcept
  {
    return TC_KMAC256_verify_short_tag(key, data, custom, tag);
  }
};
} // namespace tiny_crypto
#endif
#endif
