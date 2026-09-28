/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_AES_HPP_
#define TINY_CRYPTO_AES_HPP_

#ifndef __cplusplus
#error Do not include aes.hpp in a C project, include aes.h instead
#endif

#include <tiny_crypto/aes.h>
#include <tiny_crypto/common.hpp>

namespace tiny_crypto {

class AES {
public:
    AES() noexcept = default;
    ~AES() noexcept { TC_AES_ctx_clear(&ctx_); }
    AES(const AES&) = delete;
    AES& operator=(const AES&) = delete;

    TC_status init(const uint8_t* key, size_t key_len) noexcept {
        return key_len == TC_AES_KEYLEN ? TC_AES_init_ctx(&ctx_, key) : TC_ERROR;
    }
    template <size_t N> TC_status init(const uint8_t (&key)[N]) noexcept {
        return init(key, N);
    }

#if TC_AES_ENABLE_CBC || TC_AES_ENABLE_CTR || TC_AES_ENABLE_OFB
    TC_status init(const uint8_t* key, size_t key_len,
                   const uint8_t* iv, size_t iv_len) noexcept {
        return key_len == TC_AES_KEYLEN && iv_len == TC_AES_BLOCKLEN ?
               TC_AES_init_ctx_iv(&ctx_, key, iv) : TC_ERROR;
    }
    TC_status set_iv(const uint8_t* iv, size_t iv_len) noexcept {
        return iv_len == TC_AES_BLOCKLEN ? TC_AES_ctx_set_iv(&ctx_, iv) : TC_ERROR;
    }
    template <size_t N> TC_status set_iv(const uint8_t (&iv)[N]) noexcept {
        return set_iv(iv, N);
    }
#endif

#if TC_AES_ENABLE_ECB
    TC_status encrypt_ecb(uint8_t* block) const noexcept {
        return TC_AES_ECB_encrypt(&ctx_.key, block);
    }
    TC_status decrypt_ecb(uint8_t* block) const noexcept {
        return TC_AES_ECB_decrypt(&ctx_.key, block);
    }
#endif
#if TC_AES_ENABLE_CBC
    TC_status encrypt_cbc(uint8_t* data, size_t length) noexcept {
        return TC_AES_CBC_encrypt(&ctx_, data, length);
    }
    TC_status decrypt_cbc(uint8_t* data, size_t length) noexcept {
        return TC_AES_CBC_decrypt(&ctx_, data, length);
    }
    template <size_t N> TC_status encrypt_cbc(uint8_t (&data)[N]) noexcept {
        return encrypt_cbc(data, N);
    }
    template <size_t N> TC_status decrypt_cbc(uint8_t (&data)[N]) noexcept {
        return decrypt_cbc(data, N);
    }
#endif
#if TC_AES_ENABLE_CTR
    TC_status xcrypt_ctr(uint8_t* data, size_t length) noexcept {
        return TC_AES_CTR_crypt(&ctx_, data, length);
    }
    template <size_t N> TC_status xcrypt_ctr(uint8_t (&data)[N]) noexcept {
        return xcrypt_ctr(data, N);
    }
#endif
#if TC_AES_ENABLE_OFB
    TC_status xcrypt_ofb(uint8_t* data, size_t length) noexcept {
        return TC_AES_OFB_crypt(&ctx_, data, length);
    }
    template <size_t N> TC_status xcrypt_ofb(uint8_t (&data)[N]) noexcept {
        return xcrypt_ofb(data, N);
    }
#endif

    void clear() noexcept { TC_AES_ctx_clear(&ctx_); }
    const TC_AES_ctx& get_c_ctx() const noexcept { return ctx_; }

private:
    TC_AES_ctx ctx_{};
};

#if TC_AES_ENABLE_GCM
class GCM {
public:
    GCM() noexcept = default;
    ~GCM() noexcept { TC_AES_GCM_clear(&ctx_); }
    GCM(const GCM&) = delete;
    GCM& operator=(const GCM&) = delete;

    TC_status init(const uint8_t* key, size_t key_len,
                   const uint8_t* iv, size_t iv_len,
                   size_t tag_len = TC_AES_BLOCKLEN) noexcept {
        return key_len == TC_AES_KEYLEN ?
               TC_AES_GCM_init(&ctx_, key, iv, iv_len, tag_len) : TC_ERROR;
    }
    TC_status aad_update(const uint8_t* aad, size_t length) noexcept {
        return TC_AES_GCM_aad_update(&ctx_, aad, length);
    }
    TC_status encrypt_update(uint8_t* data, size_t length) noexcept {
        return TC_AES_GCM_encrypt_update(&ctx_, data, length);
    }
    TC_status decrypt_update(uint8_t* data, size_t length) noexcept {
        return TC_AES_GCM_decrypt_update(&ctx_, data, length);
    }
    TC_status encrypt_finish(uint8_t* tag, size_t tag_len) noexcept {
        return tag_len == ctx_.tag_len ?
               TC_AES_GCM_encrypt_finish(&ctx_, tag) : TC_ERROR;
    }
    TC_status decrypt_finish(const uint8_t* tag, size_t tag_len) noexcept {
        return tag_len == ctx_.tag_len ?
               TC_AES_GCM_decrypt_finish(&ctx_, tag) : TC_ERROR;
    }
    size_t tag_length() const noexcept { return ctx_.tag_len; }
    const TC_AES_GCM_ctx& get_c_ctx() const noexcept { return ctx_; }

private:
    TC_AES_GCM_ctx ctx_{};
};
#endif

#if TC_AES_ENABLE_CMAC
inline TC_status aes_cmac(const uint8_t* key, size_t key_len,
                          const uint8_t* message, size_t message_len,
                          uint8_t* tag, size_t tag_len) noexcept {
    return key_len == TC_AES_KEYLEN ?
           TC_AES_CMAC(key, message, message_len, tag, tag_len) : TC_ERROR;
}
#endif

#if TC_AES_ENABLE_CCM
inline TC_status ccm_encrypt(const uint8_t* key, const uint8_t* nonce,
                             size_t nonce_len, const uint8_t* aad, size_t aad_len,
                             const uint8_t* plaintext, size_t plaintext_len,
                             uint8_t* ciphertext, uint8_t* tag,
                             size_t tag_len) noexcept {
    return TC_AES_CCM_encrypt(key, nonce, nonce_len, aad, aad_len, plaintext,
                              plaintext_len, ciphertext, tag, tag_len);
}
inline TC_status ccm_decrypt(const uint8_t* key, const uint8_t* nonce,
                             size_t nonce_len, const uint8_t* aad, size_t aad_len,
                             const uint8_t* ciphertext, size_t ciphertext_len,
                             const uint8_t* tag, size_t tag_len,
                             uint8_t* plaintext) noexcept {
    return TC_AES_CCM_decrypt(key, nonce, nonce_len, aad, aad_len, ciphertext,
                              ciphertext_len, tag, tag_len, plaintext);
}
#endif

#if TC_AES_ENABLE_EAX
inline TC_status eax_encrypt(const uint8_t* key, const uint8_t* nonce,
                             size_t nonce_len, const uint8_t* aad, size_t aad_len,
                             const uint8_t* plaintext, size_t plaintext_len,
                             uint8_t* ciphertext, uint8_t* tag,
                             size_t tag_len) noexcept {
    return TC_AES_EAX_encrypt(key, nonce, nonce_len, aad, aad_len, plaintext,
                              plaintext_len, ciphertext, tag, tag_len);
}
inline TC_status eax_decrypt(const uint8_t* key, const uint8_t* nonce,
                             size_t nonce_len, const uint8_t* aad, size_t aad_len,
                             const uint8_t* ciphertext, size_t ciphertext_len,
                             const uint8_t* tag, size_t tag_len,
                             uint8_t* plaintext) noexcept {
    return TC_AES_EAX_decrypt(key, nonce, nonce_len, aad, aad_len, ciphertext,
                              ciphertext_len, tag, tag_len, plaintext);
}
#endif

#if TC_AES_ENABLE_EAX_PRIME
inline TC_status eax_prime_encrypt(const uint8_t* key, const uint8_t* cleartext,
                                   size_t cleartext_len, const uint8_t* plaintext,
                                   size_t plaintext_len, uint8_t* ciphertext,
                                   uint8_t* tag) noexcept {
    return TC_AES_EAX_PRIME_encrypt(key, cleartext, cleartext_len, plaintext,
                                    plaintext_len, ciphertext, tag);
}
inline TC_status eax_prime_decrypt(const uint8_t* key, const uint8_t* cleartext,
                                   size_t cleartext_len, const uint8_t* ciphertext,
                                   size_t ciphertext_len, const uint8_t* tag,
                                   uint8_t* plaintext) noexcept {
    return TC_AES_EAX_PRIME_decrypt(key, cleartext, cleartext_len, ciphertext,
                                    ciphertext_len, tag, plaintext);
}
#endif

#if TC_AES_ENABLE_SIV
inline TC_status siv_encrypt(const uint8_t* key, const uint8_t* const* ad,
                             const size_t* ad_lens, size_t ad_count,
                             const uint8_t* plaintext, size_t plaintext_len,
                             uint8_t* synthetic_iv,
                             uint8_t* ciphertext) noexcept {
    return TC_AES_SIV_encrypt(key, ad, ad_lens, ad_count, plaintext,
                              plaintext_len, synthetic_iv, ciphertext);
}
inline TC_status siv_decrypt(const uint8_t* key, const uint8_t* const* ad,
                             const size_t* ad_lens, size_t ad_count,
                             const uint8_t* synthetic_iv,
                             const uint8_t* ciphertext, size_t ciphertext_len,
                             uint8_t* plaintext) noexcept {
    return TC_AES_SIV_decrypt(key, ad, ad_lens, ad_count, synthetic_iv,
                              ciphertext, ciphertext_len, plaintext);
}
#endif

} /* namespace tiny_crypto */

#endif /* TINY_CRYPTO_AES_HPP_ */
