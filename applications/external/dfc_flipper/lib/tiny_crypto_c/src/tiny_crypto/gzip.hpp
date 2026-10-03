/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* GZIPDecoder for gzip.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_GZIP_HPP_
#define TINY_CRYPTO_GZIP_HPP_

#ifndef __cplusplus
#error Do not include gzip.hpp in a C project, include gzip.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/gzip.h>
#if TC_ENABLE_GZIP

namespace tiny_crypto {
/* Owns reusable scratch. Input and output remain caller-owned. */
class GZIPDecoder {
public:
  GZIPDecoder() noexcept : workspace_()
  {}
  GZIPDecoder(const GZIPDecoder&) = delete;
  GZIPDecoder& operator=(const GZIPDecoder&) = delete;

  /* TC_GZIP_decode with the owned workspace, which is wiped after each call.
   * Statuses, work and output rules match gzip.h. Calls on one decoder must
   * be serialized. */
  TC_CPP_NODISCARD TC_GZIP_result decode(TC_bytes input, size_t& work, TC_buffer output,
                                         size_t& output_length) noexcept
  {
    return TC_GZIP_decode(input, &workspace_, &work, output, &output_length);
  }

  /* Decode a whole input array into a whole output array. */
  template <size_t InputSize, size_t OutputSize>
  TC_CPP_NODISCARD TC_GZIP_result decode(const uint8_t (&input)[InputSize], size_t& work,
                                         uint8_t (&output)[OutputSize],
                                         size_t& output_length) noexcept
  {
    return decode(TC_bytes{input, InputSize}, work, TC_buffer{output, OutputSize}, output_length);
  }

private:
  TC_GZIP_workspace workspace_;
};
} // namespace tiny_crypto
#endif
#endif
