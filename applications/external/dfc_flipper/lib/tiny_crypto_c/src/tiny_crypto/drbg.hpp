/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* C++ wrapper for the SP 800-90A DRBGs. The DRBG object owns one TC_DRBG and
 * uninstantiates it on destruction. Results are the C TC_DRBG_result values.
 * Contracts, statuses and lifetimes follow drbg.h. Conventions: docs/cpp.md.
 * Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_DRBG_HPP_
#define TINY_CRYPTO_DRBG_HPP_

#ifndef __cplusplus
#error Do not include drbg.hpp in a C project, include drbg.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/drbg.h>

#if TC_ENABLE_DRBG
namespace tiny_crypto {
typedef ::TC_DRBG_config drbg_config;

// Copying or moving would duplicate the generator state and repeat output.
class DRBG {
  ::TC_DRBG state_;

public:
  DRBG() noexcept : state_{}
  {}
  ~DRBG() noexcept
  {
    uninstantiate();
  }
  DRBG(const DRBG&) = delete;
  DRBG& operator=(const DRBG&) = delete;
  DRBG(DRBG&&) = delete;
  DRBG& operator=(DRBG&&) = delete;

  TC_CPP_NODISCARD TC_DRBG_result instantiate(const drbg_config& config, TC_random_source entropy,
                                              bytes nonce, bytes personalization) noexcept
  {
    return ::TC_DRBG_instantiate(&state_, &config, entropy, nonce, personalization);
  }
  TC_CPP_NODISCARD TC_DRBG_result reseed(bytes additional = bytes{nullptr, 0}) noexcept
  {
    return ::TC_DRBG_reseed(&state_, additional);
  }
  // Fill all output.capacity bytes of output.
  TC_CPP_NODISCARD TC_DRBG_result generate(buffer output, bool prediction_resistance = false,
                                           bytes additional = bytes{nullptr, 0}) noexcept
  {
    return ::TC_DRBG_generate(&state_, output, prediction_resistance ? 1 : 0, additional);
  }
  void uninstantiate() noexcept
  {
    ::TC_DRBG_uninstantiate(&state_);
  }
  // A TC_random_source for the C APIs. This object must outlive it.
  TC_random_source random_source() noexcept
  {
    return ::TC_DRBG_random_source(&state_);
  }
};
} // namespace tiny_crypto
#endif
#endif
