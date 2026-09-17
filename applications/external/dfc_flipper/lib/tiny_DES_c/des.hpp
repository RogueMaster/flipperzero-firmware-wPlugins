/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#ifndef TINY_DES_HPP_
#define TINY_DES_HPP_

#ifndef __cplusplus
#error Do not include des.hpp in a C project, include des.h instead
#endif

#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>

extern "C" {
#include "des.h"
}

namespace tiny_des {

/* C++ RAII wrapper for Single DES */
class DES {
public:
    explicit DES(const std::array<uint8_t, 8>& key) {
        DES_init_ctx(&ctx_, key.data());
    }

#if DES_NEEDS_IV
    DES(const std::array<uint8_t, 8>& key, const std::array<uint8_t, 8>& iv) {
        DES_init_ctx_iv(&ctx_, key.data(), iv.data());
    }

    void set_iv(const std::array<uint8_t, 8>& iv) {
        DES_ctx_set_iv(&ctx_, iv.data());
    }
#endif

    ~DES() {
        DES_ctx_clear(&ctx_);
    }

    DES(const DES&) = delete;
    DES& operator=(const DES&) = delete;

#if DES_ENABLE_ECB
    void encrypt_ecb(uint8_t block[8]) const {
        DES_ECB_encrypt(&ctx_, block);
    }

    void decrypt_ecb(uint8_t block[8]) const {
        DES_ECB_decrypt(&ctx_, block);
    }
#endif

#if DES_ENABLE_CBC
    void encrypt_cbc(std::vector<uint8_t>& data) {
        if (DES_CBC_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::invalid_argument("CBC length must be a multiple of 8");
        }
    }

    void decrypt_cbc(std::vector<uint8_t>& data) {
        if (DES_CBC_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::invalid_argument("CBC length must be a multiple of 8");
        }
    }
#endif

#if DES_ENABLE_CTR
    void xcrypt_ctr(std::vector<uint8_t>& data) {
        if (DES_CTR_crypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_CTR_crypt failed (counter wrap or invalid args)");
        }
    }
#endif

#if DES_ENABLE_CFB64
    void encrypt_cfb64(std::vector<uint8_t>& data) {
        if (DES_CFB64_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_CFB64_encrypt failed");
        }
    }

    void decrypt_cfb64(std::vector<uint8_t>& data) {
        if (DES_CFB64_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_CFB64_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_CFB8
    void encrypt_cfb8(std::vector<uint8_t>& data) {
        if (DES_CFB8_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_CFB8_encrypt failed");
        }
    }

    void decrypt_cfb8(std::vector<uint8_t>& data) {
        if (DES_CFB8_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_CFB8_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_CFB1
    void encrypt_cfb1(std::vector<uint8_t>& data, size_t bit_length) {
        if (DES_CFB1_encrypt(&ctx_, data.data(), bit_length) != DES_OK) {
            throw std::runtime_error("DES_CFB1_encrypt failed");
        }
    }

    void decrypt_cfb1(std::vector<uint8_t>& data, size_t bit_length) {
        if (DES_CFB1_decrypt(&ctx_, data.data(), bit_length) != DES_OK) {
            throw std::runtime_error("DES_CFB1_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_OFB
    void xcrypt_ofb(std::vector<uint8_t>& data) {
        if (DES_OFB_crypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES_OFB_crypt failed");
        }
    }
#endif

    const DES_ctx& get_c_ctx() const { return ctx_; }

private:
    DES_ctx ctx_;
};

#if DES_ENABLE_TDES
/* C++ RAII wrapper for Triple-DES (3DES / TDES) */
class DES3 {
public:
    explicit DES3(const std::vector<uint8_t>& key) {
        if (DES3_init_ctx(&ctx_, key.data(), key.size()) != DES_OK) {
            throw std::invalid_argument("3DES key size must be 16 (2-key) or 24 (3-key) bytes");
        }
    }

#if DES_NEEDS_IV
    DES3(const std::vector<uint8_t>& key, const std::array<uint8_t, 8>& iv) {
        if (DES3_init_ctx_iv(&ctx_, key.data(), key.size(), iv.data()) != DES_OK) {
            throw std::invalid_argument("3DES key size must be 16 (2-key) or 24 (3-key) bytes");
        }
    }

    void set_iv(const std::array<uint8_t, 8>& iv) {
        DES3_ctx_set_iv(&ctx_, iv.data());
    }
#endif

    ~DES3() {
        DES3_ctx_clear(&ctx_);
    }

    DES3(const DES3&) = delete;
    DES3& operator=(const DES3&) = delete;

#if DES_ENABLE_ECB
    void encrypt_ecb(uint8_t block[8]) const {
        DES3_ECB_encrypt(&ctx_, block);
    }

    void decrypt_ecb(uint8_t block[8]) const {
        DES3_ECB_decrypt(&ctx_, block);
    }
#endif

#if DES_ENABLE_CBC
    void encrypt_cbc(std::vector<uint8_t>& data) {
        if (DES3_CBC_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::invalid_argument("CBC length must be a multiple of 8");
        }
    }

    void decrypt_cbc(std::vector<uint8_t>& data) {
        if (DES3_CBC_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::invalid_argument("CBC length must be a multiple of 8");
        }
    }
#endif

#if DES_ENABLE_CTR
    void xcrypt_ctr(std::vector<uint8_t>& data) {
        if (DES3_CTR_crypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_CTR_crypt failed (counter wrap or invalid args)");
        }
    }
#endif

#if DES_ENABLE_CFB64
    void encrypt_cfb64(std::vector<uint8_t>& data) {
        if (DES3_CFB64_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_CFB64_encrypt failed");
        }
    }

    void decrypt_cfb64(std::vector<uint8_t>& data) {
        if (DES3_CFB64_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_CFB64_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_CFB8
    void encrypt_cfb8(std::vector<uint8_t>& data) {
        if (DES3_CFB8_encrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_CFB8_encrypt failed");
        }
    }

    void decrypt_cfb8(std::vector<uint8_t>& data) {
        if (DES3_CFB8_decrypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_CFB8_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_CFB1
    void encrypt_cfb1(std::vector<uint8_t>& data, size_t bit_length) {
        if (DES3_CFB1_encrypt(&ctx_, data.data(), bit_length) != DES_OK) {
            throw std::runtime_error("DES3_CFB1_encrypt failed");
        }
    }

    void decrypt_cfb1(std::vector<uint8_t>& data, size_t bit_length) {
        if (DES3_CFB1_decrypt(&ctx_, data.data(), bit_length) != DES_OK) {
            throw std::runtime_error("DES3_CFB1_decrypt failed");
        }
    }
#endif

#if DES_ENABLE_OFB
    void xcrypt_ofb(std::vector<uint8_t>& data) {
        if (DES3_OFB_crypt(&ctx_, data.data(), data.size()) != DES_OK) {
            throw std::runtime_error("DES3_OFB_crypt failed");
        }
    }
#endif

    const DES3_ctx& get_c_ctx() const { return ctx_; }

private:
    DES3_ctx ctx_;
};
#endif /* DES_ENABLE_TDES */

#if DES_ENABLE_CMAC
/* C++ CMAC helper (full 8-byte tag) */
inline std::array<uint8_t, 8> cmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& message) {
    std::array<uint8_t, 8> out{};
    if (DES_CMAC(key.data(), key.size(),
                 message.empty() ? nullptr : message.data(), message.size(),
                 out.data(), out.size()) != DES_OK) {
        throw std::invalid_argument("DES_CMAC failed (key size or arguments)");
    }
    return out;
}
#endif /* DES_ENABLE_CMAC */

} // namespace tiny_des

#endif /* TINY_DES_HPP_ */
