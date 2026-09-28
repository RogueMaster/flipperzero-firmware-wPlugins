/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_AES_DYNAMIC_HPP_
#define TINY_CRYPTO_AES_DYNAMIC_HPP_
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/aes_dynamic.h>

namespace tiny_crypto {

class AES_dynamic {
    TC_AES_dynamic_key ctx_;
public:
    AES_dynamic() noexcept : ctx_{} {}
    ~AES_dynamic() { clear(); }
    AES_dynamic(const AES_dynamic&) = delete;
    AES_dynamic& operator=(const AES_dynamic&) = delete;
    TC_status init(const uint8_t* key, size_t length) noexcept {
        return ::TC_AES_dynamic_key_init(&ctx_, key, length);
    }
    void clear() noexcept { ::TC_AES_dynamic_key_clear(&ctx_); }
    TC_status encrypt(uint8_t (&block)[16]) const noexcept {
        return ::TC_AES_dynamic_encrypt(&ctx_, block);
    }
    TC_status decrypt(uint8_t (&block)[16]) const noexcept {
        return ::TC_AES_dynamic_decrypt(&ctx_, block);
    }
    TC_status cbc_encrypt(uint8_t (&iv)[16], uint8_t* buffer, size_t length) const noexcept {
        return ::TC_AES_dynamic_CBC_encrypt(&ctx_, iv, buffer, length);
    }
    TC_status cbc_decrypt(uint8_t (&iv)[16], uint8_t* buffer, size_t length) const noexcept {
        return ::TC_AES_dynamic_CBC_decrypt(&ctx_, iv, buffer, length);
    }
};

class AES_dynamic_CMAC {
    TC_AES_dynamic_CMAC ctx_;
public:
    AES_dynamic_CMAC() noexcept : ctx_{} {}
    ~AES_dynamic_CMAC() { clear(); }
    AES_dynamic_CMAC(const AES_dynamic_CMAC&) = delete;
    AES_dynamic_CMAC& operator=(const AES_dynamic_CMAC&) = delete;
    TC_status init(const uint8_t* key, size_t length) noexcept {
        return ::TC_AES_dynamic_CMAC_init(&ctx_, key, length);
    }
    TC_status update(const uint8_t* data, size_t length) noexcept {
        return ::TC_AES_dynamic_CMAC_update(&ctx_, data, length);
    }
    TC_status final(uint8_t (&tag)[16]) noexcept {
        return ::TC_AES_dynamic_CMAC_final(&ctx_, tag);
    }
    void clear() noexcept { ::TC_AES_dynamic_CMAC_clear(&ctx_); }
};
} // namespace tiny_crypto
#endif
