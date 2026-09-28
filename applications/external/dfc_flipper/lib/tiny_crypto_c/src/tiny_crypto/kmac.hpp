/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_KMAC_HPP_
#define TINY_CRYPTO_KMAC_HPP_
#ifndef __cplusplus
#error "Use kmac.h in C projects"
#endif
#include <tiny_crypto/kmac.h>
#if TC_ENABLE_KMAC256
namespace tiny_crypto {
class KMAC256 {
  TC_KMAC256_ctx ctx_{};
public:
  KMAC256() = default;
  KMAC256(const KMAC256&) = delete;
  KMAC256& operator=(const KMAC256&) = delete;
  ~KMAC256() { clear(); }
  TC_status init(const uint8_t* key, size_t key_len,
                 const uint8_t* custom = nullptr, size_t custom_len = 0) {
    return TC_KMAC256_init(&ctx_, key, key_len, custom, custom_len);
  }
  TC_status update(const uint8_t* data, size_t len) {
    return TC_KMAC256_update(&ctx_, data, len);
  }
  TC_status final(uint8_t* out, size_t len) {
    return TC_KMAC256_final(&ctx_, out, len);
  }
  void clear() { TC_KMAC256_ctx_clear(&ctx_); }
  static TC_status digest(const uint8_t* key, size_t key_len,
      const uint8_t* data, size_t len, const uint8_t* custom, size_t custom_len,
      uint8_t* out, size_t out_len) {
    return TC_KMAC256_digest(key, key_len, data, len, custom, custom_len,
                            out, out_len);
  }
};
}
#endif
#endif
