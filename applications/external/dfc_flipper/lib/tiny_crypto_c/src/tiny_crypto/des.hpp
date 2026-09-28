/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_DES_HPP_
#define TINY_CRYPTO_DES_HPP_

#ifndef __cplusplus
#error Do not include des.hpp in a C project, include des.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/des.h>

namespace tiny_crypto {
namespace detail {

struct tc_des_traits {
    typedef TC_DES_ctx context;
    static TC_status init(context* c, const uint8_t* k, size_t n) noexcept {
        return n == TC_DES_KEYLEN ? TC_DES_init_ctx(c, k) : TC_ERROR;
    }
#if TC_DES_NEEDS_IV
    static TC_status init_iv(context* c, const uint8_t* k, size_t n,
                             const uint8_t* iv) noexcept {
        return n == TC_DES_KEYLEN ? TC_DES_init_ctx_iv(c, k, iv) : TC_ERROR;
    }
    static TC_status set_iv(context* c, const uint8_t* iv) noexcept {
        return TC_DES_ctx_set_iv(c, iv);
    }
#endif
    static void clear(context* c) noexcept { TC_DES_ctx_clear(c); }
#if TC_DES_ENABLE_ECB
    static TC_status encrypt_ecb(const context* c, uint8_t* b) noexcept {
        return TC_DES_ECB_encrypt(c, b);
    }
    static TC_status decrypt_ecb(const context* c, uint8_t* b) noexcept {
        return TC_DES_ECB_decrypt(c, b);
    }
#endif
#if TC_DES_ENABLE_CBC
    static TC_status encrypt_cbc(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CBC_encrypt(c, b, n);
    }
    static TC_status decrypt_cbc(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CBC_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CTR
    static TC_status ctr(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CTR_crypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB64
    static TC_status encrypt_cfb64(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB64_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb64(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB64_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB8
    static TC_status encrypt_cfb8(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB8_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb8(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB8_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB1
    static TC_status encrypt_cfb1(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB1_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb1(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_CFB1_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_OFB
    static TC_status ofb(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES_OFB_crypt(c, b, n);
    }
#endif
};

#if TC_DES_ENABLE_TDES
struct tc_des3_traits {
    typedef TC_DES3_ctx context;
    static TC_status init(context* c, const uint8_t* k, size_t n) noexcept {
        return TC_DES3_init_ctx(c, k, n);
    }
#if TC_DES_NEEDS_IV
    static TC_status init_iv(context* c, const uint8_t* k, size_t n,
                             const uint8_t* iv) noexcept {
        return TC_DES3_init_ctx_iv(c, k, n, iv);
    }
    static TC_status set_iv(context* c, const uint8_t* iv) noexcept {
        return TC_DES3_ctx_set_iv(c, iv);
    }
#endif
    static void clear(context* c) noexcept { TC_DES3_ctx_clear(c); }
#if TC_DES_ENABLE_ECB
    static TC_status encrypt_ecb(const context* c, uint8_t* b) noexcept {
        return TC_DES3_ECB_encrypt(c, b);
    }
    static TC_status decrypt_ecb(const context* c, uint8_t* b) noexcept {
        return TC_DES3_ECB_decrypt(c, b);
    }
#endif
#if TC_DES_ENABLE_CBC
    static TC_status encrypt_cbc(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CBC_encrypt(c, b, n);
    }
    static TC_status decrypt_cbc(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CBC_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CTR
    static TC_status ctr(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CTR_crypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB64
    static TC_status encrypt_cfb64(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB64_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb64(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB64_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB8
    static TC_status encrypt_cfb8(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB8_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb8(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB8_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_CFB1
    static TC_status encrypt_cfb1(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB1_encrypt(c, b, n);
    }
    static TC_status decrypt_cfb1(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_CFB1_decrypt(c, b, n);
    }
#endif
#if TC_DES_ENABLE_OFB
    static TC_status ofb(context* c, uint8_t* b, size_t n) noexcept {
        return TC_DES3_OFB_crypt(c, b, n);
    }
#endif
};
#endif

} /* namespace detail */

template <class Traits>
class basic_des {
public:
    basic_des() noexcept = default;
    ~basic_des() noexcept { Traits::clear(&ctx_); }
    basic_des(const basic_des&) = delete;
    basic_des& operator=(const basic_des&) = delete;

    TC_status init(const uint8_t* key, size_t key_len) noexcept {
        return Traits::init(&ctx_, key, key_len);
    }
    template <size_t N> TC_status init(const uint8_t (&key)[N]) noexcept {
        return init(key, N);
    }
#if TC_DES_NEEDS_IV
    TC_status init(const uint8_t* key, size_t key_len,
                   const uint8_t* iv, size_t iv_len) noexcept {
        return iv_len == TC_DES_BLOCKLEN ?
               Traits::init_iv(&ctx_, key, key_len, iv) : TC_ERROR;
    }
    TC_status set_iv(const uint8_t* iv, size_t iv_len) noexcept {
        return iv_len == TC_DES_BLOCKLEN ? Traits::set_iv(&ctx_, iv) : TC_ERROR;
    }
#endif
#if TC_DES_ENABLE_ECB
    TC_status encrypt_ecb(uint8_t* block) const noexcept {
        return Traits::encrypt_ecb(&ctx_, block);
    }
    TC_status decrypt_ecb(uint8_t* block) const noexcept {
        return Traits::decrypt_ecb(&ctx_, block);
    }
#endif
#if TC_DES_ENABLE_CBC
    TC_status encrypt_cbc(uint8_t* data, size_t n) noexcept {
        return Traits::encrypt_cbc(&ctx_, data, n);
    }
    TC_status decrypt_cbc(uint8_t* data, size_t n) noexcept {
        return Traits::decrypt_cbc(&ctx_, data, n);
    }
#endif
#if TC_DES_ENABLE_CTR
    TC_status xcrypt_ctr(uint8_t* data, size_t n) noexcept {
        return Traits::ctr(&ctx_, data, n);
    }
#endif
#if TC_DES_ENABLE_CFB64
    TC_status encrypt_cfb64(uint8_t* data, size_t n) noexcept {
        return Traits::encrypt_cfb64(&ctx_, data, n);
    }
    TC_status decrypt_cfb64(uint8_t* data, size_t n) noexcept {
        return Traits::decrypt_cfb64(&ctx_, data, n);
    }
#endif
#if TC_DES_ENABLE_CFB8
    TC_status encrypt_cfb8(uint8_t* data, size_t n) noexcept {
        return Traits::encrypt_cfb8(&ctx_, data, n);
    }
    TC_status decrypt_cfb8(uint8_t* data, size_t n) noexcept {
        return Traits::decrypt_cfb8(&ctx_, data, n);
    }
#endif
#if TC_DES_ENABLE_CFB1
    TC_status encrypt_cfb1(uint8_t* data, size_t data_len,
                           size_t bits) noexcept {
        return bits <= 8u * data_len ?
               Traits::encrypt_cfb1(&ctx_, data, bits) : TC_ERROR;
    }
    TC_status decrypt_cfb1(uint8_t* data, size_t data_len,
                           size_t bits) noexcept {
        return bits <= 8u * data_len ?
               Traits::decrypt_cfb1(&ctx_, data, bits) : TC_ERROR;
    }
    template <size_t N>
    TC_status encrypt_cfb1(uint8_t (&data)[N], size_t bits) noexcept {
        return encrypt_cfb1(data, N, bits);
    }
    template <size_t N>
    TC_status decrypt_cfb1(uint8_t (&data)[N], size_t bits) noexcept {
        return decrypt_cfb1(data, N, bits);
    }
#endif
#if TC_DES_ENABLE_OFB
    TC_status xcrypt_ofb(uint8_t* data, size_t n) noexcept {
        return Traits::ofb(&ctx_, data, n);
    }
#endif
    void clear() noexcept { Traits::clear(&ctx_); }
    const typename Traits::context& get_c_ctx() const noexcept { return ctx_; }

private:
    typename Traits::context ctx_{};
};

typedef basic_des<detail::tc_des_traits> DES;
#if TC_DES_ENABLE_TDES
typedef basic_des<detail::tc_des3_traits> DES3;
#endif

#if TC_DES_ENABLE_CMAC
inline TC_status des_cmac(const uint8_t* key, size_t key_len,
                          const uint8_t* message, size_t message_len,
                          uint8_t* tag, size_t tag_len) noexcept {
    return TC_DES_CMAC(key, key_len, message, message_len, tag, tag_len);
}
#endif

} /* namespace tiny_crypto */

#endif /* TINY_CRYPTO_DES_HPP_ */
