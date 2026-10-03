/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key wrap functions for aes_kw.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_AES_KW_HPP_
#define TINY_CRYPTO_AES_KW_HPP_

#ifndef __cplusplus
#error Do not include aes_kw.hpp in a C project, include aes_kw.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/aes_kw.h>
#if TC_ENABLE_AES && TC_AES_ENABLE_KW

namespace tiny_crypto {

#if TC_AES_ENABLE_KW
/* SP 800-38F KW and KWP under a borrowed KEK span. The KEK length follows
 * TC_AES_KW_KEK_LENGTH_SUPPORTED. key_data_length receives the KWP key data
 * length on TC_OK only. */
TC_CPP_NODISCARD inline TC_status aes_kw_wrap(bytes kek, bytes key_data, buffer wrapped) noexcept
{
  return ::TC_AES_KW_wrap(kek, key_data, wrapped);
}
TC_CPP_NODISCARD inline TC_status aes_kw_unwrap(bytes kek, bytes wrapped, buffer key_data) noexcept
{
  return ::TC_AES_KW_unwrap(kek, wrapped, key_data);
}
TC_CPP_NODISCARD inline TC_status aes_kwp_wrap(bytes kek, bytes key_data, buffer wrapped) noexcept
{
  return ::TC_AES_KWP_wrap(kek, key_data, wrapped);
}
TC_CPP_NODISCARD inline TC_status aes_kwp_unwrap(bytes kek, bytes wrapped, buffer key_data,
                                                 size_t& key_data_length) noexcept
{
  return ::TC_AES_KWP_unwrap(kek, wrapped, key_data, &key_data_length);
}
#endif

} // namespace tiny_crypto

#endif
#endif
