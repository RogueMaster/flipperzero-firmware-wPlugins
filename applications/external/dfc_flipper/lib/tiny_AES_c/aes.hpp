/*
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */
#ifndef TINY_AES_HPP_
#define TINY_AES_HPP_

#ifndef __cplusplus
#error Do not include aes.hpp in a C project, include aes.h instead
#endif

#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <cstddef>

extern "C" {
#include "aes.h"
}

namespace tiny_aes {

/* C++ RAII wrapper for core AES context */
class AES {
public:
    explicit AES(const std::vector<uint8_t>& key) {
        if (key.size() != AES_KEYLEN) {
            throw std::invalid_argument("Invalid key size for active AES mode");
        }
        AES_init_ctx(&ctx_, key.data());
    }

    template <size_t N>
    explicit AES(const std::array<uint8_t, N>& key) {
        if (N != AES_KEYLEN) {
            throw std::invalid_argument("Invalid key size for active AES mode");
        }
        AES_init_ctx(&ctx_, key.data());
    }

#if (defined(AES_ENABLE_CBC) && AES_ENABLE_CBC == 1) || \
    (defined(AES_ENABLE_CTR) && AES_ENABLE_CTR == 1) || \
    (defined(AES_ENABLE_OFB) && AES_ENABLE_OFB == 1)
    AES(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
        if (key.size() != AES_KEYLEN || iv.size() != AES_BLOCKLEN) {
            throw std::invalid_argument("Invalid key or IV length");
        }
        AES_init_ctx_iv(&ctx_, key.data(), iv.data());
    }

    template <size_t N, size_t M>
    AES(const std::array<uint8_t, N>& key, const std::array<uint8_t, M>& iv) {
        if (N != AES_KEYLEN || M != AES_BLOCKLEN) {
            throw std::invalid_argument("Invalid key or IV length");
        }
        AES_init_ctx_iv(&ctx_, key.data(), iv.data());
    }

    void set_iv(const std::vector<uint8_t>& iv) {
        if (iv.size() != AES_BLOCKLEN) {
            throw std::invalid_argument("IV length must be 16 bytes");
        }
        AES_ctx_set_iv(&ctx_, iv.data());
    }

    template <size_t M>
    void set_iv(const std::array<uint8_t, M>& iv) {
        if (M != AES_BLOCKLEN) {
            throw std::invalid_argument("IV length must be 16 bytes");
        }
        AES_ctx_set_iv(&ctx_, iv.data());
    }
#endif

    ~AES() {
        AES_ctx_clear(&ctx_);
    }

    AES(const AES&) = delete;
    AES& operator=(const AES&) = delete;

#if defined(AES_ENABLE_ECB) && AES_ENABLE_ECB == 1
    void encrypt_ecb(uint8_t block[AES_BLOCKLEN]) const {
        AES_ECB_encrypt(&ctx_, block);
    }

    void decrypt_ecb(uint8_t block[AES_BLOCKLEN]) const {
        AES_ECB_decrypt(&ctx_, block);
    }
#endif

#if defined(AES_ENABLE_CBC) && AES_ENABLE_CBC == 1
    void encrypt_cbc(std::vector<uint8_t>& data) {
        if (AES_CBC_encrypt(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::invalid_argument("CBC length must be a non-zero multiple of 16 bytes");
        }
    }

    void decrypt_cbc(std::vector<uint8_t>& data) {
        if (AES_CBC_decrypt(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::invalid_argument("CBC length must be a non-zero multiple of 16 bytes");
        }
    }
#endif

#if defined(AES_ENABLE_CTR) && AES_ENABLE_CTR == 1
    void xcrypt_ctr(std::vector<uint8_t>& data) {
        if (AES_CTR_crypt(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::runtime_error("AES_CTR_crypt failed (counter wrap or invalid arguments)");
        }
    }
#endif

#if defined(AES_ENABLE_OFB) && AES_ENABLE_OFB == 1
    void xcrypt_ofb(std::vector<uint8_t>& data) {
        if (AES_OFB_crypt(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::runtime_error("AES_OFB_crypt failed");
        }
    }
#endif

    const AES_ctx& get_c_ctx() const { return ctx_; }

private:
    AES_ctx ctx_;
};

#if defined(AES_ENABLE_GCM) && AES_ENABLE_GCM == 1
/* C++ RAII wrapper for streaming AES-GCM */
class GCM {
public:
    GCM(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, size_t tag_len = 16) {
        if (key.size() != AES_KEYLEN || iv.empty()) {
            throw std::invalid_argument("Invalid key or IV length for GCM");
        }
        if (AES_GCM_init(&ctx_, key.data(), iv.data(), iv.size(), tag_len) != AES_OK) {
            throw std::invalid_argument("AES_GCM_init failed");
        }
    }

    ~GCM() {
        AES_GCM_clear(&ctx_);
    }

    GCM(const GCM&) = delete;
    GCM& operator=(const GCM&) = delete;

    void aad_update(const std::vector<uint8_t>& aad) {
        if (AES_GCM_aad_update(&ctx_, aad.data(), aad.size()) != AES_OK) {
            throw std::runtime_error("AES_GCM_aad_update failed");
        }
    }

    void encrypt_update(std::vector<uint8_t>& data) {
        if (AES_GCM_encrypt_update(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::runtime_error("AES_GCM_encrypt_update failed");
        }
    }

    void decrypt_update(std::vector<uint8_t>& data) {
        if (AES_GCM_decrypt_update(&ctx_, data.data(), data.size()) != AES_OK) {
            throw std::runtime_error("AES_GCM_decrypt_update failed");
        }
    }

    std::vector<uint8_t> encrypt_finish() {
        std::vector<uint8_t> tag(16);
        if (AES_GCM_encrypt_finish(&ctx_, tag.data()) != AES_OK) {
            throw std::runtime_error("AES_GCM_encrypt_finish failed");
        }
        return tag;
    }

    void decrypt_finish(const std::vector<uint8_t>& tag) {
        if (AES_GCM_decrypt_finish(&ctx_, tag.data()) != AES_OK) {
            throw std::runtime_error("AES_GCM_decrypt_finish failed (authentication failure)");
        }
    }

    const AES_GCM_ctx& get_c_ctx() const { return ctx_; }

private:
    AES_GCM_ctx ctx_;
};
#endif

#if defined(AES_ENABLE_CMAC) && AES_ENABLE_CMAC == 1
inline std::vector<uint8_t> cmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& message, size_t tag_len = 16) {
    if (key.size() != AES_KEYLEN) {
        throw std::invalid_argument("Invalid key length for CMAC");
    }
    std::vector<uint8_t> tag(tag_len);
    if (AES_CMAC(key.data(), message.empty() ? nullptr : message.data(), message.size(), tag.data(), tag.size()) != AES_OK) {
        throw std::invalid_argument("AES_CMAC failed");
    }
    return tag;
}
#endif

} // namespace tiny_aes

#endif // TINY_AES_HPP_
