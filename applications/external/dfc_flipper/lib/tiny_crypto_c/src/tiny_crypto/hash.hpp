/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_HASH_HPP_
#define TINY_CRYPTO_HASH_HPP_

#ifndef __cplusplus
#error Do not include hash.hpp in a C project, include hash.h instead
#endif

#include <tiny_crypto/common.hpp>
#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || \
    TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.h>
#endif
#if TC_ENABLE_MD5
#include <tiny_crypto/md5.h>
#endif

namespace tiny_crypto {
namespace detail {

#define TINY_CRYPTO_HASH_TRAITS(name, C_NAME, context_type, digest_len) \
struct name { \
    using context = context_type; \
    static constexpr size_t digest_size = digest_len; \
    static TC_status init(context* ctx) { return TC_##C_NAME##_init(ctx); } \
    static TC_status update(context* ctx, const uint8_t* data, size_t length) { \
        return TC_##C_NAME##_update(ctx, data, length); \
    } \
    static TC_status final(context* ctx, uint8_t* digest) { \
        return TC_##C_NAME##_final(ctx, digest); \
    } \
    static TC_status digest(const uint8_t* data, size_t length, uint8_t* out) { \
        return TC_##C_NAME##_digest(data, length, out); \
    } \
    static void clear(context* ctx) { TC_##C_NAME##_ctx_clear(ctx); } \
};

#define TINY_CRYPTO_HMAC_TRAITS(name, C_NAME, context_type, digest_len) \
struct name { \
    using context = context_type; \
    static constexpr size_t digest_size = digest_len; \
    static TC_status init(context* ctx, const uint8_t* key, size_t length) { \
        return TC_HMAC_##C_NAME##_init(ctx, key, length); \
    } \
    static TC_status update(context* ctx, const uint8_t* data, size_t length) { \
        return TC_HMAC_##C_NAME##_update(ctx, data, length); \
    } \
    static TC_status final(context* ctx, uint8_t* tag) { \
        return TC_HMAC_##C_NAME##_final(ctx, tag); \
    } \
    static TC_status digest(const uint8_t* key, size_t key_len, \
                            const uint8_t* data, size_t length, uint8_t* tag) { \
        return TC_HMAC_##C_NAME##_digest(key, key_len, data, length, tag, \
                                         digest_size); \
    } \
    static TC_status verify(const uint8_t* key, size_t key_len, \
                            const uint8_t* data, size_t length, \
                            const uint8_t* tag, size_t tag_len) { \
        return TC_HMAC_##C_NAME##_verify(key, key_len, data, length, tag, \
                                         tag_len); \
    } \
    static void clear(context* ctx) { TC_HMAC_##C_NAME##_ctx_clear(ctx); } \
};

#if TC_ENABLE_MD5
TINY_CRYPTO_HASH_TRAITS(tc_md5_traits, MD5, TC_MD5_ctx, TC_MD5_DIGESTLEN)
#endif
#if TC_ENABLE_SHA1
TINY_CRYPTO_HASH_TRAITS(tc_sha1_traits, SHA1, TC_SHA1_ctx, TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_SHA224
TINY_CRYPTO_HASH_TRAITS(tc_sha224_traits, SHA224, TC_SHA224_ctx,
                       TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_SHA256
TINY_CRYPTO_HASH_TRAITS(tc_sha256_traits, SHA256, TC_SHA256_ctx,
                       TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_SHA384
TINY_CRYPTO_HASH_TRAITS(tc_sha384_traits, SHA384, TC_SHA384_ctx,
                       TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_SHA512
TINY_CRYPTO_HASH_TRAITS(tc_sha512_traits, SHA512, TC_SHA512_ctx,
                       TC_SHA512_DIGESTLEN)
#endif

#if TC_ENABLE_HMAC && TC_ENABLE_SHA1
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha1_traits, SHA1, TC_HMAC_SHA1_ctx,
                        TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA224
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha224_traits, SHA224, TC_HMAC_SHA224_ctx,
                        TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA256
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha256_traits, SHA256, TC_HMAC_SHA256_ctx,
                        TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA384
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha384_traits, SHA384, TC_HMAC_SHA384_ctx,
                        TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA512
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha512_traits, SHA512, TC_HMAC_SHA512_ctx,
                        TC_SHA512_DIGESTLEN)
#endif

#undef TINY_CRYPTO_HMAC_TRAITS
#undef TINY_CRYPTO_HASH_TRAITS

} // namespace detail

/* Each hash retains its concrete C context. */
template <class Traits>
class basic_hash {
public:
    static const size_t digest_size = Traits::digest_size;

    basic_hash() { (void)Traits::init(&ctx_); }
    ~basic_hash() { Traits::clear(&ctx_); }
    basic_hash(const basic_hash&) = delete;
    basic_hash& operator=(const basic_hash&) = delete;

    TC_status reset() { return Traits::init(&ctx_); }

    TC_status update(const uint8_t* data, size_t length) {
        return Traits::update(&ctx_, data, length);
    }
    template <size_t N>
    TC_status update(const uint8_t (&data)[N]) { return update(data, N); }

    TC_status finish(uint8_t* out, size_t out_len) {
        if (out_len != digest_size)
            return TC_ERROR;
        TC_status status = Traits::final(&ctx_, out);
        if (status == TC_OK)
            status = Traits::init(&ctx_);
        return status;
    }
    template <size_t N>
    TC_status finish(uint8_t (&out)[N]) {
        return finish(out, N);
    }

    static TC_status digest(const uint8_t* data, size_t length,
                            uint8_t* out, size_t out_len) {
        if (out_len != digest_size)
            return TC_ERROR;
        return Traits::digest(data, length, out);
    }
    template <size_t InLen, size_t OutLen>
    static TC_status digest(const uint8_t (&data)[InLen],
                            uint8_t (&out)[OutLen]) {
        return digest(data, InLen, out, OutLen);
    }

private:
    typename Traits::context ctx_;
};

template <class Traits>
class basic_hmac {
public:
    static const size_t tag_size = Traits::digest_size;

    basic_hmac() : active_(false) {}
    basic_hmac(const uint8_t* key, size_t key_len) : active_(false) {
        active_ = Traits::init(&ctx_, key, key_len) == TC_OK;
    }
    template <size_t N>
    explicit basic_hmac(const uint8_t (&key)[N]) : active_(false) {
        active_ = Traits::init(&ctx_, key, N) == TC_OK;
    }

    ~basic_hmac() { Traits::clear(&ctx_); }
    basic_hmac(const basic_hmac&) = delete;
    basic_hmac& operator=(const basic_hmac&) = delete;

    TC_status init(const uint8_t* key, size_t key_len) {
        const TC_status status = Traits::init(&ctx_, key, key_len);
        active_ = status == TC_OK;
        return status;
    }
    template <size_t N>
    TC_status init(const uint8_t (&key)[N]) { return init(key, N); }

    TC_status update(const uint8_t* data, size_t length) {
        return active_ ? Traits::update(&ctx_, data, length) : TC_ERROR;
    }
    template <size_t N>
    TC_status update(const uint8_t (&data)[N]) { return update(data, N); }

    TC_status finish(uint8_t* out, size_t out_len) {
        if (!active_ || out_len != tag_size)
            return TC_ERROR;
        const TC_status status = Traits::final(&ctx_, out);
        active_ = false;
        return status;
    }
    template <size_t N>
    TC_status finish(uint8_t (&out)[N]) { return finish(out, N); }

    static TC_status mac(const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t length,
                         uint8_t* out, size_t out_len) {
        if (out_len != tag_size)
            return TC_ERROR;
        return Traits::digest(key, key_len, data, length, out);
    }

    static TC_status verify(const uint8_t* key, size_t key_len,
                            const uint8_t* data, size_t length,
                            const uint8_t* tag, size_t tag_len) {
        return Traits::verify(key, key_len, data, length, tag, tag_len);
    }

private:
    typename Traits::context ctx_;
    bool active_;
};

#if TC_ENABLE_MD5
typedef basic_hash<detail::tc_md5_traits> MD5;
#endif
#if TC_ENABLE_SHA1
typedef basic_hash<detail::tc_sha1_traits> SHA1;
#endif
#if TC_ENABLE_SHA224
typedef basic_hash<detail::tc_sha224_traits> SHA224;
#endif
#if TC_ENABLE_SHA256
typedef basic_hash<detail::tc_sha256_traits> SHA256;
#endif
#if TC_ENABLE_SHA384
typedef basic_hash<detail::tc_sha384_traits> SHA384;
#endif
#if TC_ENABLE_SHA512
typedef basic_hash<detail::tc_sha512_traits> SHA512;
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA1
typedef basic_hmac<detail::tc_hmac_sha1_traits> HMAC_SHA1;
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA224
typedef basic_hmac<detail::tc_hmac_sha224_traits> HMAC_SHA224;
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA256
typedef basic_hmac<detail::tc_hmac_sha256_traits> HMAC_SHA256;
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA384
typedef basic_hmac<detail::tc_hmac_sha384_traits> HMAC_SHA384;
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA512
typedef basic_hmac<detail::tc_hmac_sha512_traits> HMAC_SHA512;
#endif

} // namespace tiny_crypto

#endif /* TINY_CRYPTO_HASH_HPP_ */
