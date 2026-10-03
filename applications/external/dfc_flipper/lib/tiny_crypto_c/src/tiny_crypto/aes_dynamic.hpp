/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* AESDynamic and AESDynamicCMAC classes for aes_dynamic.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_AES_DYNAMIC_HPP_
#define TINY_CRYPTO_AES_DYNAMIC_HPP_

#ifndef __cplusplus
#error Do not include aes_dynamic.hpp in a C project, include aes_dynamic.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/aes_dynamic.h>
#if TC_ENABLE_AES && TC_AES_ENABLE_DYNAMIC

namespace tiny_crypto {

/* AES with a key length chosen at init: 16, 24 or 32 bytes. The key is a
 * borrowed span disjoint from the object. A failed init leaves the object
 * unkeyed. The destructor clears the key schedule. */
class AESDynamic {
  TC_AES_dynamic_key ctx_;

public:
  AESDynamic() noexcept : ctx_{}
  {}
  ~AESDynamic() noexcept
  {
    clear();
  }
  AESDynamic(const AESDynamic&) = delete;
  AESDynamic& operator=(const AESDynamic&) = delete;
  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    return ::TC_AES_dynamic_key_init(&ctx_, TC_bytes{key.data, key.length});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }
  void clear() noexcept
  {
    ::TC_AES_dynamic_key_clear(&ctx_);
  }
  TC_CPP_NODISCARD TC_status encrypt(uint8_t (&block)[16]) const noexcept
  {
    return ::TC_AES_dynamic_encrypt(&ctx_, TC_buffer{block, TC_AES_BLOCKLEN});
  }
  TC_CPP_NODISCARD TC_status decrypt(uint8_t (&block)[16]) const noexcept
  {
    return ::TC_AES_dynamic_decrypt(&ctx_, TC_buffer{block, TC_AES_BLOCKLEN});
  }
  TC_CPP_NODISCARD TC_status cbc_encrypt(uint8_t (&iv)[16], buffer data) const noexcept
  {
    return ::TC_AES_dynamic_CBC_encrypt(&ctx_, TC_buffer{iv, TC_AES_BLOCKLEN}, data);
  }
  TC_CPP_NODISCARD TC_status cbc_decrypt(uint8_t (&iv)[16], buffer data) const noexcept
  {
    return ::TC_AES_dynamic_CBC_decrypt(&ctx_, TC_buffer{iv, TC_AES_BLOCKLEN}, data);
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status cbc_encrypt(uint8_t (&iv)[16], uint8_t (&data)[N]) const noexcept
  {
    return cbc_encrypt(iv, buffer{data, N});
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status cbc_decrypt(uint8_t (&iv)[16], uint8_t (&data)[N]) const noexcept
  {
    return cbc_decrypt(iv, buffer{data, N});
  }
};

/* Streaming AES-CMAC with a key length chosen at init. finish writes the full
 * 16-byte tag and consumes the key. A failed init, finish and clear leave the
 * object unkeyed. The destructor clears the context. */
class AESDynamicCMAC {
  TC_AES_dynamic_CMAC ctx_;

public:
  AESDynamicCMAC() noexcept : ctx_{}
  {}
  ~AESDynamicCMAC() noexcept
  {
    clear();
  }
  AESDynamicCMAC(const AESDynamicCMAC&) = delete;
  AESDynamicCMAC& operator=(const AESDynamicCMAC&) = delete;
  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    return ::TC_AES_dynamic_CMAC_init(&ctx_, TC_bytes{key.data, key.length});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }
  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return ::TC_AES_dynamic_CMAC_update(&ctx_, TC_bytes{data.data, data.length});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&data)[N]) noexcept
  {
    return update(bytes{data, N});
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&tag)[16]) noexcept
  {
    return ::TC_AES_dynamic_CMAC_final(&ctx_, TC_buffer{tag, TC_AES_BLOCKLEN});
  }
  void clear() noexcept
  {
    ::TC_AES_dynamic_CMAC_clear(&ctx_);
  }
};
} // namespace tiny_crypto
#endif
#endif
