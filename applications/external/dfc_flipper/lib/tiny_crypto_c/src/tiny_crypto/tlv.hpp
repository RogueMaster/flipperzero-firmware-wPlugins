/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* TLVReader for tlv.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_TLV_HPP_
#define TINY_CRYPTO_TLV_HPP_

#ifndef __cplusplus
#error Do not include tlv.hpp in a C project, include tlv.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/tlv.h>
#if TC_ENABLE_TLV

namespace tiny_crypto {
/* This wrapper owns the cursor. The caller owns the bytes it reads. Explicit init
 * keeps initialization failures visible without exceptions or allocations. */
class TLVReader {
public:
  TLVReader() noexcept : reader_(), ready_(false)
  {}
  /* Start a root reader with TC_TLV_reader_init. input stays borrowed and
   * unchanged while the reader is used. A failure leaves this reader unusable
   * until the next successful init. */
  TC_CPP_NODISCARD TC_TLV_result init(TC_bytes input, TC_TLV_profile profile,
                                      const TC_TLV_limits& limits) noexcept
  {
    /* A failed init must not leave the previous cursor usable. */
    ready_ = false;
    TC_TLV_result result = TC_TLV_reader_init(&reader_, input, profile, &limits);
    if (result == TC_TLV_OK)
      ready_ = true;
    return result;
  }
  /* Borrow a whole C array. */
  template <size_t Size>
  TC_CPP_NODISCARD TC_TLV_result init(const uint8_t (&input)[Size], TC_TLV_profile profile,
                                      const TC_TLV_limits& limits) noexcept
  {
    return init(TC_bytes{input, Size}, profile, limits);
  }
  /* Read the template of an element returned by parent. Padding and
   * truncation inside the template return TC_TLV_INVALID. parent may be this
   * reader. A failure leaves this reader unusable. */
  TC_CPP_NODISCARD TC_TLV_result init_child(const TLVReader& parent,
                                            const TC_TLV_element& element) noexcept
  {
    const bool parent_ready = parent.ready_;
    ready_ = false;
    if (!parent_ready)
      return TC_TLV_ARGUMENT;
    TC_TLV_result result = TC_TLV_reader_child(&reader_, &parent.reader_, &element);
    if (result == TC_TLV_OK)
      ready_ = true;
    return result;
  }
  /* TC_TLV_next. An unusable reader returns TC_TLV_ARGUMENT. */
  TC_CPP_NODISCARD TC_TLV_result next(TC_TLV_element& element) noexcept
  {
    return ready_ ? TC_TLV_next(&reader_, &element) : TC_TLV_ARGUMENT;
  }

private:
  TC_TLV_reader reader_;
  bool ready_;
};
} // namespace tiny_crypto
#endif
#endif
