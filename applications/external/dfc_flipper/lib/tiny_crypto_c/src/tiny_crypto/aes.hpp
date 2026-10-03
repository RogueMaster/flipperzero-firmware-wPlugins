/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* AES, GCM and AES-CMAC classes and the one-shot AEAD and MAC functions for aes.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_AES_HPP_
#define TINY_CRYPTO_AES_HPP_

#ifndef __cplusplus
#error Do not include aes.hpp in a C project, include aes.h instead
#endif

#include <tiny_crypto/aes.h>
#include <tiny_crypto/common.hpp>
#if TC_ENABLE_AES

namespace tiny_crypto {

/* AES block cipher for the configured TC_AES_KEYLEN. Keys and IVs are borrowed
 * spans that must stay disjoint from the object. A wrong key or IV length
 * returns TC_ERROR and clears the object, as does any other init failure.
 * Later cipher calls then return TC_ERROR until the next successful init.
 * Mode calls transform data in place under the rules of aes.h. The destructor
 * clears the context. */
class AES {
public:
  AES() noexcept = default;
  ~AES() noexcept
  {
    TC_AES_ctx_clear(&ctx_);
  }
  AES(const AES&) = delete;
  AES& operator=(const AES&) = delete;

  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    if (key.length != TC_AES_KEYLEN) {
      TC_AES_ctx_clear(&ctx_);
      return TC_ERROR;
    }
    return TC_AES_init(&ctx_, TC_bytes{key.data, TC_AES_KEYLEN});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }

#if TC_AES_ENABLE_CBC || TC_AES_ENABLE_CTR || TC_AES_ENABLE_OFB
  /* Key the object and load the first IV of TC_AES_BLOCKLEN bytes. */
  TC_CPP_NODISCARD TC_status init(bytes key, bytes iv) noexcept
  {
    if (key.length != TC_AES_KEYLEN || iv.length != TC_AES_BLOCKLEN) {
      TC_AES_ctx_clear(&ctx_);
      return TC_ERROR;
    }
    TC_status status = TC_AES_init(&ctx_, TC_bytes{key.data, TC_AES_KEYLEN});
    if (status == TC_OK)
      status = TC_AES_set_iv(&ctx_, TC_bytes{iv.data, TC_AES_BLOCKLEN});
    if (status != TC_OK)
      TC_AES_ctx_clear(&ctx_);
    return status;
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N],
                                  const uint8_t (&iv)[TC_AES_BLOCKLEN]) noexcept
  {
    return init(bytes{key, N}, bytes{iv, TC_AES_BLOCKLEN});
  }
  /* Start a new message. A wrong length returns TC_ERROR and keeps the key. */
  TC_CPP_NODISCARD TC_status set_iv(bytes iv) noexcept
  {
    return iv.length == TC_AES_BLOCKLEN ? TC_AES_set_iv(&ctx_, TC_bytes{iv.data, TC_AES_BLOCKLEN})
                                        : TC_ERROR;
  }
  TC_CPP_NODISCARD TC_status set_iv(const uint8_t (&iv)[TC_AES_BLOCKLEN]) noexcept
  {
    return set_iv(bytes{iv, TC_AES_BLOCKLEN});
  }
#endif

#if TC_AES_ENABLE_ECB
  TC_CPP_NODISCARD TC_status encrypt_ecb(uint8_t* block) const noexcept
  {
    return TC_AES_ECB_encrypt(&ctx_.key, TC_buffer{block, TC_AES_BLOCKLEN});
  }
  TC_CPP_NODISCARD TC_status decrypt_ecb(uint8_t* block) const noexcept
  {
    return TC_AES_ECB_decrypt(&ctx_.key, TC_buffer{block, TC_AES_BLOCKLEN});
  }
#endif
#if TC_AES_ENABLE_CBC
  TC_CPP_NODISCARD TC_status encrypt_cbc(buffer data) noexcept
  {
    return TC_AES_CBC_encrypt(&ctx_, data);
  }
  TC_CPP_NODISCARD TC_status decrypt_cbc(buffer data) noexcept
  {
    return TC_AES_CBC_decrypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status encrypt_cbc(uint8_t (&data)[N]) noexcept
  {
    return encrypt_cbc(buffer{data, N});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status decrypt_cbc(uint8_t (&data)[N]) noexcept
  {
    return decrypt_cbc(buffer{data, N});
  }
#endif
#if TC_AES_ENABLE_CTR
  TC_CPP_NODISCARD TC_status xcrypt_ctr(buffer data) noexcept
  {
    return TC_AES_CTR_crypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status xcrypt_ctr(uint8_t (&data)[N]) noexcept
  {
    return xcrypt_ctr(buffer{data, N});
  }
#endif
#if TC_AES_ENABLE_OFB
  TC_CPP_NODISCARD TC_status xcrypt_ofb(buffer data) noexcept
  {
    return TC_AES_OFB_crypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status xcrypt_ofb(uint8_t (&data)[N]) noexcept
  {
    return xcrypt_ofb(buffer{data, N});
  }
#endif

  void clear() noexcept
  {
    TC_AES_ctx_clear(&ctx_);
  }
  const TC_AES_ctx& get_c_ctx() const noexcept
  {
    return ctx_;
  }

private:
  TC_AES_ctx ctx_{};
};

#if TC_AES_ENABLE_GCM
/* Streaming GCM encryption (SP 800-38D). init takes a TC_AES_KEYLEN-byte key
 * as a span or an array and fixes the tag length. A wrong key length or any
 * other init failure clears the object. Supply all AAD before the first
 * encrypt_update, which encrypts data in place. encrypt_finish writes exactly
 * tag_length() bytes and consumes the key. Decrypt with the one-shot gcm_decrypt, which verifies
 * the tag before it releases plaintext. clear and the destructor wipe the key
 * schedule and the authentication state. */
class GCM {
public:
  GCM() noexcept = default;
  ~GCM() noexcept
  {
    TC_AES_GCM_ctx_clear(&ctx_);
  }
  GCM(const GCM&) = delete;
  GCM& operator=(const GCM&) = delete;

  /* tag_len is 12..16 bytes. */
  TC_CPP_NODISCARD TC_status init(bytes key, bytes iv, size_t tag_len = TC_AES_BLOCKLEN) noexcept
  {
    if (key.length != TC_AES_KEYLEN) {
      TC_AES_GCM_ctx_clear(&ctx_);
      return TC_ERROR;
    }
    return TC_AES_GCM_init(&ctx_, TC_bytes{key.data, TC_AES_KEYLEN}, iv, tag_len);
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N], bytes iv,
                                  size_t tag_len = TC_AES_BLOCKLEN) noexcept
  {
    return init(bytes{key, N}, iv, tag_len);
  }
  /* tag_len is 4 or 8 bytes under the SP 800-38D Appendix C packet limits. */
  TC_CPP_NODISCARD TC_status init_short_tag(bytes key, bytes iv, size_t tag_len) noexcept
  {
    if (key.length != TC_AES_KEYLEN) {
      TC_AES_GCM_ctx_clear(&ctx_);
      return TC_ERROR;
    }
    return TC_AES_GCM_init_short_tag(&ctx_, TC_bytes{key.data, TC_AES_KEYLEN}, iv, tag_len);
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status init_short_tag(const uint8_t (&key)[N], bytes iv,
                                            size_t tag_len) noexcept
  {
    return init_short_tag(bytes{key, N}, iv, tag_len);
  }
  TC_CPP_NODISCARD TC_status aad_update(bytes aad) noexcept
  {
    return TC_AES_GCM_aad_update(&ctx_, TC_bytes{aad.data, aad.length});
  }
  TC_CPP_NODISCARD TC_status encrypt_update(buffer data) noexcept
  {
    return TC_AES_GCM_encrypt_update(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status encrypt_update(uint8_t (&data)[N]) noexcept
  {
    return encrypt_update(buffer{data, N});
  }
  /* tag.capacity must equal tag_length(). Another capacity returns TC_ERROR
   * and keeps the state. */
  TC_CPP_NODISCARD TC_status encrypt_finish(buffer tag) noexcept
  {
    return tag.capacity == ctx_.tag_len
               ? TC_AES_GCM_encrypt_finish(&ctx_, TC_buffer{tag.data, tag.capacity})
               : TC_ERROR;
  }
  template <size_t N> TC_CPP_NODISCARD TC_status encrypt_finish(uint8_t (&tag)[N]) noexcept
  {
    return encrypt_finish(buffer{tag, N});
  }
  TC_CPP_NODISCARD size_t tag_length() const noexcept
  {
    return ctx_.tag_len;
  }
  void clear() noexcept
  {
    TC_AES_GCM_ctx_clear(&ctx_);
  }
  const TC_AES_GCM_ctx& get_c_ctx() const noexcept
  {
    return ctx_;
  }

private:
  TC_AES_GCM_ctx ctx_{};
};
#endif

#if TC_AES_ENABLE_CMAC
/* One-shot AES-CMAC (SP 800-38B) over a TC_AES_KEYLEN-byte key. aes_cmac
 * writes tag.capacity bytes, from TC_MIN_TAG_LEN to TC_AES_CMAC_TAG_MAX, and
 * verify compares tag.length bytes in constant time. The _short_tag forms take
 * 1..TC_MIN_TAG_LEN - 1 bytes. Status values follow TC_AES_CMAC and
 * TC_AES_CMAC_verify. A wrong key length returns TC_ERROR, and every argument
 * error leaves the tag unchanged. */
TC_CPP_NODISCARD inline TC_status aes_cmac(bytes key, bytes message, buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CMAC(TC_bytes{key.data, TC_AES_KEYLEN}, TC_bytes{message.data, message.length},
                     TC_buffer{tag.data, tag.capacity});
}
TC_CPP_NODISCARD inline TC_status aes_cmac_verify(bytes key, bytes message, bytes tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CMAC_verify(TC_bytes{key.data, TC_AES_KEYLEN},
                            TC_bytes{message.data, message.length}, TC_bytes{tag.data, tag.length});
}
TC_CPP_NODISCARD inline TC_status aes_cmac_short_tag(bytes key, bytes message, buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CMAC_short_tag(TC_bytes{key.data, TC_AES_KEYLEN},
                               TC_bytes{message.data, message.length},
                               TC_buffer{tag.data, tag.capacity});
}
TC_CPP_NODISCARD inline TC_status aes_cmac_verify_short_tag(bytes key, bytes message,
                                                            bytes tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CMAC_verify_short_tag(TC_bytes{key.data, TC_AES_KEYLEN},
                                      TC_bytes{message.data, message.length},
                                      TC_bytes{tag.data, tag.length});
}

/* Streaming AES-CMAC. init takes a TC_AES_KEYLEN-byte key. finish writes the
 * full TC_AES_CMAC_TAG_MAX-byte tag and consumes the key. A failed init, a
 * finish and clear leave the object unkeyed, and update and finish then
 * return TC_ERROR until the next successful init. Compare a received tag with
 * aes_cmac_verify or tiny_crypto::ct_equal. The destructor clears the
 * context. */
class AESCMAC {
public:
  static const size_t tag_size = TC_AES_CMAC_TAG_MAX;

  AESCMAC() noexcept = default;
  ~AESCMAC() noexcept
  {
    TC_AES_CMAC_ctx_clear(&ctx_);
  }
  AESCMAC(const AESCMAC&) = delete;
  AESCMAC& operator=(const AESCMAC&) = delete;

  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    TC_status status = TC_ERROR;
    if (key.length == TC_AES_KEYLEN)
      status = TC_AES_CMAC_init(&ctx_, TC_bytes{key.data, TC_AES_KEYLEN});
    if (status != TC_OK)
      TC_AES_CMAC_ctx_clear(&ctx_);
    return status;
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }
  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return TC_AES_CMAC_update(&ctx_, TC_bytes{data.data, data.length});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&data)[N]) noexcept
  {
    return update(bytes{data, N});
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&tag)[TC_AES_CMAC_TAG_MAX]) noexcept
  {
    return TC_AES_CMAC_final(&ctx_, TC_buffer{tag, TC_AES_CMAC_TAG_MAX});
  }
  void clear() noexcept
  {
    TC_AES_CMAC_ctx_clear(&ctx_);
  }

private:
  TC_AES_CMAC_ctx ctx_{};
};
#endif

/* The one-shot AEAD wrappers take a sized key and check its length before the
 * C call. A wrong length returns TC_ERROR and leaves every output unchanged.
 * GCM, CCM, EAX and EAX' take TC_AES_KEYLEN bytes. SIV takes
 * TC_AES_SIV_KEYLEN. Otherwise they follow the one-shot AEAD contract in
 * aes.h. */
#if TC_AES_ENABLE_GCM
TC_CPP_NODISCARD inline TC_status gcm_encrypt(bytes key, bytes iv, bytes aad, bytes plaintext,
                                              buffer ciphertext, buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_GCM_encrypt(TC_bytes{key.data, TC_AES_KEYLEN}, iv, aad, plaintext, ciphertext, tag);
}
TC_CPP_NODISCARD inline TC_status gcm_decrypt(bytes key, bytes iv, bytes aad, bytes ciphertext,
                                              bytes tag, buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_GCM_decrypt(TC_bytes{key.data, TC_AES_KEYLEN}, iv, aad, ciphertext, tag, plaintext);
}
TC_CPP_NODISCARD inline TC_status gcm_encrypt_short_tag(bytes key, bytes iv, bytes aad,
                                                        bytes plaintext, buffer ciphertext,
                                                        buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_GCM_encrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, iv, aad, plaintext,
                                      ciphertext, tag);
}
TC_CPP_NODISCARD inline TC_status gcm_decrypt_short_tag(bytes key, bytes iv, bytes aad,
                                                        bytes ciphertext, bytes tag,
                                                        buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_GCM_decrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, iv, aad, ciphertext, tag,
                                      plaintext);
}
#endif

#if TC_AES_ENABLE_CCM
TC_CPP_NODISCARD inline TC_status ccm_encrypt(bytes key, bytes nonce, bytes aad, bytes plaintext,
                                              buffer ciphertext, buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CCM_encrypt(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, plaintext, ciphertext,
                            tag);
}
TC_CPP_NODISCARD inline TC_status ccm_decrypt(bytes key, bytes nonce, bytes aad, bytes ciphertext,
                                              bytes tag, buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CCM_decrypt(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, ciphertext, tag,
                            plaintext);
}
TC_CPP_NODISCARD inline TC_status ccm_encrypt_short_tag(bytes key, bytes nonce, bytes aad,
                                                        bytes plaintext, buffer ciphertext,
                                                        buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CCM_encrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, plaintext,
                                      ciphertext, tag);
}
TC_CPP_NODISCARD inline TC_status ccm_decrypt_short_tag(bytes key, bytes nonce, bytes aad,
                                                        bytes ciphertext, bytes tag,
                                                        buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_CCM_decrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, ciphertext,
                                      tag, plaintext);
}
#endif

#if TC_AES_ENABLE_EAX
TC_CPP_NODISCARD inline TC_status eax_encrypt(bytes key, bytes nonce, bytes aad, bytes plaintext,
                                              buffer ciphertext, buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_encrypt(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, plaintext, ciphertext,
                            tag);
}
TC_CPP_NODISCARD inline TC_status eax_decrypt(bytes key, bytes nonce, bytes aad, bytes ciphertext,
                                              bytes tag, buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_decrypt(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, ciphertext, tag,
                            plaintext);
}
TC_CPP_NODISCARD inline TC_status eax_encrypt_short_tag(bytes key, bytes nonce, bytes aad,
                                                        bytes plaintext, buffer ciphertext,
                                                        buffer tag) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_encrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, plaintext,
                                      ciphertext, tag);
}
TC_CPP_NODISCARD inline TC_status eax_decrypt_short_tag(bytes key, bytes nonce, bytes aad,
                                                        bytes ciphertext, bytes tag,
                                                        buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_decrypt_short_tag(TC_bytes{key.data, TC_AES_KEYLEN}, nonce, aad, ciphertext,
                                      tag, plaintext);
}
#endif

#if TC_AES_ENABLE_EAX_PRIME
TC_CPP_NODISCARD inline TC_status
eax_prime_encrypt(bytes key, bytes cleartext, bytes plaintext, buffer ciphertext,
                  uint8_t (&tag)[TC_AES_EAX_PRIME_TAG_LEN]) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_PRIME_encrypt(TC_bytes{key.data, TC_AES_KEYLEN}, cleartext, plaintext,
                                  ciphertext, TC_buffer{tag, TC_AES_EAX_PRIME_TAG_LEN});
}
TC_CPP_NODISCARD inline TC_status eax_prime_decrypt(bytes key, bytes cleartext, bytes ciphertext,
                                                    const uint8_t (&tag)[TC_AES_EAX_PRIME_TAG_LEN],
                                                    buffer plaintext) noexcept
{
  if (key.length != TC_AES_KEYLEN)
    return TC_ERROR;
  return TC_AES_EAX_PRIME_decrypt(TC_bytes{key.data, TC_AES_KEYLEN}, cleartext, ciphertext,
                                  TC_bytes{tag, TC_AES_EAX_PRIME_TAG_LEN}, plaintext);
}
#endif

#if TC_AES_ENABLE_SIV
TC_CPP_NODISCARD inline TC_status siv_encrypt(bytes key, const bytes* ad, size_t ad_count,
                                              bytes plaintext,
                                              uint8_t (&synthetic_iv)[TC_AES_SIV_V_LEN],
                                              buffer ciphertext) noexcept
{
  if (key.length != TC_AES_SIV_KEYLEN)
    return TC_ERROR;
  return TC_AES_SIV_encrypt(TC_bytes{key.data, TC_AES_SIV_KEYLEN}, ad, ad_count, plaintext,
                            TC_buffer{synthetic_iv, TC_AES_SIV_V_LEN}, ciphertext);
}
TC_CPP_NODISCARD inline TC_status siv_decrypt(bytes key, const bytes* ad, size_t ad_count,
                                              const uint8_t (&synthetic_iv)[TC_AES_SIV_V_LEN],
                                              bytes ciphertext, buffer plaintext) noexcept
{
  if (key.length != TC_AES_SIV_KEYLEN)
    return TC_ERROR;
  return TC_AES_SIV_decrypt(TC_bytes{key.data, TC_AES_SIV_KEYLEN}, ad, ad_count,
                            TC_bytes{synthetic_iv, TC_AES_SIV_V_LEN}, ciphertext, plaintext);
}
#endif

} /* namespace tiny_crypto */

#endif
#endif /* TINY_CRYPTO_AES_HPP_ */
