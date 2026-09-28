/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_GZIP_HPP_
#define TINY_CRYPTO_GZIP_HPP_
#include <tiny_crypto/gzip.h>

namespace tiny_crypto {
/* Owns reusable scratch. Input and output remain caller-owned. */
class GZIPDecoder {
 public:
  GZIPDecoder() : workspace_() {}
  GZIPDecoder(const GZIPDecoder&) = delete;
  GZIPDecoder& operator=(const GZIPDecoder&) = delete;

  TC_GZIP_result decode(const uint8_t* input, size_t length,
      uint8_t* output, size_t capacity, size_t& work, size_t& output_length) {
    return TC_GZIP_decode(input,length,output,capacity,&workspace_,&work,&output_length);
  }

  template<size_t InputSize, size_t OutputSize>
  TC_GZIP_result decode(const uint8_t (&input)[InputSize], uint8_t (&output)[OutputSize],
      size_t& work, size_t& output_length) {
    return decode(input,InputSize,output,OutputSize,work,output_length);
  }

 private:
  TC_GZIP_workspace workspace_;
};
}
#endif
