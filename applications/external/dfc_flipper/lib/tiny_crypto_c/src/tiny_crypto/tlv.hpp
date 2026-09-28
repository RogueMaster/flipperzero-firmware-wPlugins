/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_TLV_HPP_
#define TINY_CRYPTO_TLV_HPP_
#include <tiny_crypto/tlv.h>

namespace tiny_crypto {
/* This wrapper owns the cursor only, not the bytes it reads. Explicit init
 * keeps initialization failures visible without exceptions or allocations. */
class TLVReader {
 public:
  TLVReader() : reader_(), ready_(false) {}
  TC_TLV_result init(const uint8_t* data, size_t length, TC_TLV_profile profile,
                     const TC_TLV_limits& limits) {
    TC_TLV_result result = TC_TLV_reader_init(&reader_, data, length, profile, &limits);
    if (result == TC_TLV_OK) ready_ = true;
    return result;
  }
  TC_TLV_result next(TC_TLV_element& element) {
    return ready_ ? TC_TLV_next(&reader_, &element) : TC_TLV_ARGUMENT;
  }
 private:
  TC_TLV_reader reader_;
  bool ready_;
};
}
#endif
