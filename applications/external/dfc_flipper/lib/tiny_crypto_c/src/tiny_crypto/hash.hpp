/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* Hash, HMAC and MD5 classes for hash.h and md5.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_HASH_HPP_
#define TINY_CRYPTO_HASH_HPP_

#ifndef __cplusplus
#error Do not include hash.hpp in a C project, include hash.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/hash.h>
#include <tiny_crypto/md5.h>

namespace tiny_crypto {
namespace detail {

#define TINY_CRYPTO_HASH_TRAITS(name, C_NAME, context_type, digest_len)                            \
  struct name {                                                                                    \
    using context = context_type;                                                                  \
    static constexpr size_t digest_size = digest_len;                                              \
    static TC_status init(context* ctx) noexcept                                                   \
    {                                                                                              \
      return TC_##C_NAME##_init(ctx);                                                              \
    }                                                                                              \
    static TC_status update(context* ctx, bytes data) noexcept                                     \
    {                                                                                              \
      return TC_##C_NAME##_update(ctx, data);                                                      \
    }                                                                                              \
    static TC_status final(context* ctx, uint8_t* digest) noexcept                                 \
    {                                                                                              \
      return TC_##C_NAME##_final(ctx, digest);                                                     \
    }                                                                                              \
    static TC_status digest(bytes data, uint8_t* out) noexcept                                     \
    {                                                                                              \
      return TC_##C_NAME##_digest(data, out);                                                      \
    }                                                                                              \
    static void clear(context* ctx) noexcept                                                       \
    {                                                                                              \
      TC_##C_NAME##_ctx_clear(ctx);                                                                \
    }                                                                                              \
  };

#define TINY_CRYPTO_HMAC_TRAITS(name, C_NAME, context_type, digest_len)                            \
  struct name {                                                                                    \
    using context = context_type;                                                                  \
    static constexpr size_t digest_size = digest_len;                                              \
    static TC_status init(context* ctx, bytes key) noexcept                                        \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_init(ctx, key);                                                    \
    }                                                                                              \
    static TC_status update(context* ctx, bytes data) noexcept                                     \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_update(ctx, data);                                                 \
    }                                                                                              \
    static TC_status final(context* ctx, uint8_t* tag) noexcept                                    \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_final(ctx, tag);                                                   \
    }                                                                                              \
    static TC_status digest(bytes key, bytes data, buffer tag) noexcept                            \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_digest(key, data, tag);                                            \
    }                                                                                              \
    static TC_status digest_short_tag(bytes key, bytes data, buffer tag) noexcept                  \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_digest_short_tag(key, data, tag);                                  \
    }                                                                                              \
    static TC_status verify(bytes key, bytes data, bytes tag) noexcept                             \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_verify(key, data, tag);                                            \
    }                                                                                              \
    static TC_status verify_short_tag(bytes key, bytes data, bytes tag) noexcept                   \
    {                                                                                              \
      return TC_HMAC_##C_NAME##_verify_short_tag(key, data, tag);                                  \
    }                                                                                              \
    static void clear(context* ctx) noexcept                                                       \
    {                                                                                              \
      TC_HMAC_##C_NAME##_ctx_clear(ctx);                                                           \
    }                                                                                              \
  };

#if TC_ENABLE_MD5
TINY_CRYPTO_HASH_TRAITS(tc_md5_traits, MD5, TC_MD5_ctx, TC_MD5_DIGESTLEN)
#endif
#if TC_ENABLE_SHA1
TINY_CRYPTO_HASH_TRAITS(tc_sha1_traits, SHA1, TC_SHA1_ctx, TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_SHA224
TINY_CRYPTO_HASH_TRAITS(tc_sha224_traits, SHA224, TC_SHA224_ctx, TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_SHA256
TINY_CRYPTO_HASH_TRAITS(tc_sha256_traits, SHA256, TC_SHA256_ctx, TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_SHA384
TINY_CRYPTO_HASH_TRAITS(tc_sha384_traits, SHA384, TC_SHA384_ctx, TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_SHA512
TINY_CRYPTO_HASH_TRAITS(tc_sha512_traits, SHA512, TC_SHA512_ctx, TC_SHA512_DIGESTLEN)
#endif

#if TC_ENABLE_HMAC && TC_ENABLE_SHA1
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha1_traits, SHA1, TC_HMAC_SHA1_ctx, TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA224
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha224_traits, SHA224, TC_HMAC_SHA224_ctx, TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA256
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha256_traits, SHA256, TC_HMAC_SHA256_ctx, TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA384
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha384_traits, SHA384, TC_HMAC_SHA384_ctx, TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_HMAC && TC_ENABLE_SHA512
TINY_CRYPTO_HMAC_TRAITS(tc_hmac_sha512_traits, SHA512, TC_HMAC_SHA512_ctx, TC_SHA512_DIGESTLEN)
#endif

#undef TINY_CRYPTO_HMAC_TRAITS
#undef TINY_CRYPTO_HASH_TRAITS

} // namespace detail

/* Streaming hash over its concrete C context. Inputs are borrowed byte spans
 * and C arrays. The constructor starts a message. finish writes the digest and
 * starts the next message, so one object hashes many messages. The destructor
 * clears the context. A digest buffer must hold exactly digest_size bytes, and
 * another capacity returns TC_ERROR with the buffer unchanged. */
template <class Traits> class basic_hash {
public:
  static const size_t digest_size = Traits::digest_size;

  basic_hash() noexcept
  {
    (void)Traits::init(&ctx_);
  }
  ~basic_hash() noexcept
  {
    Traits::clear(&ctx_);
  }
  basic_hash(const basic_hash&) = delete;
  basic_hash& operator=(const basic_hash&) = delete;

  TC_CPP_NODISCARD TC_status reset() noexcept
  {
    return Traits::init(&ctx_);
  }

  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return Traits::update(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&data)[N]) noexcept
  {
    return update(bytes{data, N});
  }

  TC_CPP_NODISCARD TC_status finish(buffer out) noexcept
  {
    if (out.capacity != digest_size)
      return TC_ERROR;
    TC_status status = Traits::final(&ctx_, out.data);
    if (status == TC_OK)
      status = Traits::init(&ctx_);
    return status;
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&out)[Traits::digest_size]) noexcept
  {
    return finish(buffer{out, digest_size});
  }

  TC_CPP_NODISCARD static TC_status digest(bytes data, buffer out) noexcept
  {
    if (out.capacity != digest_size)
      return TC_ERROR;
    return Traits::digest(data, out.data);
  }
  template <size_t N>
  TC_CPP_NODISCARD static TC_status digest(const uint8_t (&data)[N],
                                           uint8_t (&out)[Traits::digest_size]) noexcept
  {
    return digest(bytes{data, N}, buffer{out, digest_size});
  }

private:
  typename Traits::context ctx_{};
};

/* Streaming HMAC over its concrete C context. The C context records whether
 * a key is loaded. A default-constructed object, a failed init and a
 * completed finish all leave it unkeyed, and update and finish then return
 * TC_ERROR until the next successful init. The destructor clears the context.
 * finish writes exactly tag_size bytes. mac writes out.capacity bytes and
 * verify compares tag.length bytes, each from the greater of
 * TC_HMAC_MIN_TAG_LEN and TC_MIN_TAG_LEN to tag_size. */
template <class Traits> class basic_hmac {
public:
  static const size_t tag_size = Traits::digest_size;

  basic_hmac() noexcept = default;
  explicit basic_hmac(bytes key) noexcept
  {
    (void)Traits::init(&ctx_, key);
  }
  template <size_t N> explicit basic_hmac(const uint8_t (&key)[N]) noexcept
  {
    (void)Traits::init(&ctx_, bytes{key, N});
  }

  ~basic_hmac() noexcept
  {
    Traits::clear(&ctx_);
  }
  basic_hmac(const basic_hmac&) = delete;
  basic_hmac& operator=(const basic_hmac&) = delete;

  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    return Traits::init(&ctx_, key);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }

  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return Traits::update(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&data)[N]) noexcept
  {
    return update(bytes{data, N});
  }

  /* out.capacity must equal tag_size. finish consumes the key. */
  TC_CPP_NODISCARD TC_status finish(buffer out) noexcept
  {
    if (out.capacity != tag_size)
      return TC_ERROR;
    return Traits::final(&ctx_, out.data);
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&out)[Traits::digest_size]) noexcept
  {
    return finish(buffer{out, tag_size});
  }

  /* out.capacity is TC_HMAC_MIN_TAG_LEN..tag_size. A shorter capacity keeps
   * the leading bytes of the full tag. */
  TC_CPP_NODISCARD static TC_status mac(bytes key, bytes data, buffer out) noexcept
  {
    return Traits::digest(key, data, out);
  }
  TC_CPP_NODISCARD static TC_status mac(bytes key, bytes data,
                                        uint8_t (&out)[Traits::digest_size]) noexcept
  {
    return mac(key, data, buffer{out, tag_size});
  }

  /* Compares tag in constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR. */
  TC_CPP_NODISCARD static TC_status verify(bytes key, bytes data, bytes tag) noexcept
  {
    return Traits::verify(key, data, tag);
  }
  TC_CPP_NODISCARD static TC_status mac_short_tag(bytes key, bytes data, buffer out) noexcept
  {
    return Traits::digest_short_tag(key, data, out);
  }
  TC_CPP_NODISCARD static TC_status verify_short_tag(bytes key, bytes data, bytes tag) noexcept
  {
    return Traits::verify_short_tag(key, data, tag);
  }

private:
  typename Traits::context ctx_{};
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
