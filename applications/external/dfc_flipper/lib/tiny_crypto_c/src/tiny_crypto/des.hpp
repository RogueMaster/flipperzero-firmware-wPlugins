/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* DES, DESCMAC and DESISO9797 classes and the one-shot MAC functions for des.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_DES_HPP_
#define TINY_CRYPTO_DES_HPP_

#ifndef __cplusplus
#error Do not include des.hpp in a C project, include des.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/des.h>
#if TC_ENABLE_DES

namespace tiny_crypto {

/* DES or TDEA cipher. init selects single DES for an 8-byte key and TDEA for a
 * 16- or 24-byte bundle when TC_DES_ENABLE_TDES is set. A failed init leaves
 * the object unkeyed, and later cipher calls return TC_ERROR. Keys and IVs are
 * borrowed spans that must stay disjoint from the object. Mode calls transform
 * data in place under the rules of des.h. The destructor clears the context. */
class DES {
public:
  DES() noexcept = default;
  ~DES() noexcept
  {
    TC_DES_ctx_clear(&ctx_);
  }
  DES(const DES&) = delete;
  DES& operator=(const DES&) = delete;

  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    return TC_DES_init(&ctx_, key);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }
#if TC_DES_NEEDS_IV
  /* Key the object and load the first IV. iv.length must be TC_DES_BLOCKLEN.
   * Any failure clears the object. */
  TC_CPP_NODISCARD TC_status init(bytes key, bytes iv) noexcept
  {
    if (iv.length != TC_DES_BLOCKLEN) {
      TC_DES_ctx_clear(&ctx_);
      return TC_ERROR;
    }
    TC_status status = TC_DES_init(&ctx_, key);
    if (status == TC_OK)
      status = TC_DES_set_iv(&ctx_, iv);
    if (status != TC_OK)
      TC_DES_ctx_clear(&ctx_);
    return status;
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N],
                                  const uint8_t (&iv)[TC_DES_BLOCKLEN]) noexcept
  {
    return init(bytes{key, N}, bytes{iv, TC_DES_BLOCKLEN});
  }
  /* Start a new message. A wrong length returns TC_ERROR and keeps the key. */
  TC_CPP_NODISCARD TC_status set_iv(bytes iv) noexcept
  {
    return iv.length == TC_DES_BLOCKLEN ? TC_DES_set_iv(&ctx_, iv) : TC_ERROR;
  }
  TC_CPP_NODISCARD TC_status set_iv(const uint8_t (&iv)[TC_DES_BLOCKLEN]) noexcept
  {
    return set_iv(bytes{iv, TC_DES_BLOCKLEN});
  }
#endif
#if TC_DES_ENABLE_ECB
  TC_CPP_NODISCARD TC_status encrypt_ecb(uint8_t* block) const noexcept
  {
    return TC_DES_ECB_encrypt(&ctx_, TC_buffer{block, TC_DES_BLOCKLEN});
  }
  TC_CPP_NODISCARD TC_status decrypt_ecb(uint8_t* block) const noexcept
  {
    return TC_DES_ECB_decrypt(&ctx_, TC_buffer{block, TC_DES_BLOCKLEN});
  }
#endif
#if TC_DES_ENABLE_CBC
  TC_CPP_NODISCARD TC_status encrypt_cbc(buffer data) noexcept
  {
    return TC_DES_CBC_encrypt(&ctx_, data);
  }
  TC_CPP_NODISCARD TC_status decrypt_cbc(buffer data) noexcept
  {
    return TC_DES_CBC_decrypt(&ctx_, data);
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
#if TC_DES_ENABLE_CTR
  TC_CPP_NODISCARD TC_status xcrypt_ctr(buffer data) noexcept
  {
    return TC_DES_CTR_crypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status xcrypt_ctr(uint8_t (&data)[N]) noexcept
  {
    return xcrypt_ctr(buffer{data, N});
  }
#endif
#if TC_DES_ENABLE_CFB64
  /* A call whose length is not a multiple of 8 ends the message. See des.h. */
  TC_CPP_NODISCARD TC_status encrypt_cfb64(buffer data) noexcept
  {
    return TC_DES_CFB64_encrypt(&ctx_, data);
  }
  TC_CPP_NODISCARD TC_status decrypt_cfb64(buffer data) noexcept
  {
    return TC_DES_CFB64_decrypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status encrypt_cfb64(uint8_t (&data)[N]) noexcept
  {
    return encrypt_cfb64(buffer{data, N});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status decrypt_cfb64(uint8_t (&data)[N]) noexcept
  {
    return decrypt_cfb64(buffer{data, N});
  }
#endif
#if TC_DES_ENABLE_CFB8
  TC_CPP_NODISCARD TC_status encrypt_cfb8(buffer data) noexcept
  {
    return TC_DES_CFB8_encrypt(&ctx_, data);
  }
  TC_CPP_NODISCARD TC_status decrypt_cfb8(buffer data) noexcept
  {
    return TC_DES_CFB8_decrypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status encrypt_cfb8(uint8_t (&data)[N]) noexcept
  {
    return encrypt_cfb8(buffer{data, N});
  }
  template <size_t N> TC_CPP_NODISCARD TC_status decrypt_cfb8(uint8_t (&data)[N]) noexcept
  {
    return decrypt_cfb8(buffer{data, N});
  }
#endif
#if TC_DES_ENABLE_CFB1
  TC_CPP_NODISCARD TC_status encrypt_cfb1(buffer data, size_t bits) noexcept
  {
    return cfb1_fits(data.capacity, bits) ? TC_DES_CFB1_encrypt(&ctx_, data, bits) : TC_ERROR;
  }
  TC_CPP_NODISCARD TC_status decrypt_cfb1(buffer data, size_t bits) noexcept
  {
    return cfb1_fits(data.capacity, bits) ? TC_DES_CFB1_decrypt(&ctx_, data, bits) : TC_ERROR;
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status encrypt_cfb1(uint8_t (&data)[N], size_t bits) noexcept
  {
    return encrypt_cfb1(buffer{data, N}, bits);
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status decrypt_cfb1(uint8_t (&data)[N], size_t bits) noexcept
  {
    return decrypt_cfb1(buffer{data, N}, bits);
  }
#endif
#if TC_DES_ENABLE_OFB
  TC_CPP_NODISCARD TC_status xcrypt_ofb(buffer data) noexcept
  {
    return TC_DES_OFB_crypt(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status xcrypt_ofb(uint8_t (&data)[N]) noexcept
  {
    return xcrypt_ofb(buffer{data, N});
  }
#endif
  void clear() noexcept
  {
    TC_DES_ctx_clear(&ctx_);
  }
  const TC_DES_ctx& get_c_ctx() const noexcept
  {
    return ctx_;
  }

private:
#if TC_DES_ENABLE_CFB1
  /* bits must fit in data_len bytes. */
  static bool cfb1_fits(size_t data_len, size_t bits) noexcept
  {
    return bits / 8u < data_len || (bits / 8u == data_len && bits % 8u == 0);
  }
#endif
  TC_DES_ctx ctx_{};
};

#if TC_DES_ENABLE_CMAC
/* One-shot DES/TDEA-CMAC (SP 800-38B) over a key of TC_DES_KEYLEN,
 * TC_DES_KEYLEN_2KEY or TC_DES_KEYLEN_3KEY bytes. des_cmac writes
 * tag.capacity bytes, from TC_MIN_TAG_LEN to TC_DES_CMAC_TAG_MAX, and verify
 * compares tag.length bytes in constant time. The _short_tag forms take
 * 1..TC_MIN_TAG_LEN - 1 bytes. Status values follow TC_DES_CMAC and
 * TC_DES_CMAC_verify. An argument error leaves the tag unchanged. */
TC_CPP_NODISCARD inline TC_status des_cmac(bytes key, bytes message, buffer tag) noexcept
{
  return TC_DES_CMAC(key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_cmac_verify(bytes key, bytes message, bytes tag) noexcept
{
  return TC_DES_CMAC_verify(key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_cmac_short_tag(bytes key, bytes message, buffer tag) noexcept
{
  return TC_DES_CMAC_short_tag(key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_cmac_verify_short_tag(bytes key, bytes message,
                                                            bytes tag) noexcept
{
  return TC_DES_CMAC_verify_short_tag(key, message, tag);
}

/* Streaming DES/TDEA-CMAC. init takes the key lengths of des_cmac. finish
 * writes the full TC_DES_CMAC_TAG_MAX-byte tag and consumes the key. A failed
 * init, a finish and clear leave the object unkeyed, and update and finish
 * then return TC_ERROR until the next successful init. Compare a received tag
 * with des_cmac_verify or tiny_crypto::ct_equal. The destructor clears the
 * context. */
class DESCMAC {
public:
  static const size_t tag_size = TC_DES_CMAC_TAG_MAX;

  DESCMAC() noexcept = default;
  ~DESCMAC() noexcept
  {
    TC_DES_CMAC_ctx_clear(&ctx_);
  }
  DESCMAC(const DESCMAC&) = delete;
  DESCMAC& operator=(const DESCMAC&) = delete;

  TC_CPP_NODISCARD TC_status init(bytes key) noexcept
  {
    const TC_status status = TC_DES_CMAC_init(&ctx_, key);
    if (status != TC_OK)
      TC_DES_CMAC_ctx_clear(&ctx_);
    return status;
  }
  template <size_t N> TC_CPP_NODISCARD TC_status init(const uint8_t (&key)[N]) noexcept
  {
    return init(bytes{key, N});
  }
  TC_CPP_NODISCARD TC_status update(bytes data) noexcept
  {
    return TC_DES_CMAC_update(&ctx_, data);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&data)[N]) noexcept
  {
    return update(bytes{data, N});
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&tag)[TC_DES_CMAC_TAG_MAX]) noexcept
  {
    return TC_DES_CMAC_final(&ctx_, TC_buffer{tag, TC_DES_CMAC_TAG_MAX});
  }
  void clear() noexcept
  {
    TC_DES_CMAC_ctx_clear(&ctx_);
  }

private:
  TC_DES_CMAC_ctx ctx_{};
};
#endif

#if TC_DES_ENABLE_ISO9797
/* One-shot ISO/IEC 9797-1 MAC algorithm 1 or 3. Standard ALG3 takes a
 * 16-byte key; ALG3_3KEY_EXTENSION explicitly selects the 24-byte form.
 * des_iso9797_mac writes the full TC_DES_BLOCKLEN-byte MAC and verify
 * compares a full MAC. The _short_tag forms take the leading 4..7 bytes.
 * Verify returns TC_OK, TC_MISMATCH for a wrong MAC, or TC_ERROR for an
 * argument, key, padding or length error. MAC leaves the tag unchanged on
 * error. des.h documents the algorithms, paddings and key rules. */
TC_CPP_NODISCARD inline TC_status des_iso9797_mac(TC_DES_ISO9797_algorithm algorithm,
                                                  TC_DES_ISO9797_padding padding, bytes key,
                                                  bytes message, buffer tag) noexcept
{
  return TC_DES_ISO9797_MAC(algorithm, padding, key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_iso9797_verify(TC_DES_ISO9797_algorithm algorithm,
                                                     TC_DES_ISO9797_padding padding, bytes key,
                                                     bytes message, bytes tag) noexcept
{
  return TC_DES_ISO9797_verify(algorithm, padding, key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_iso9797_mac_short_tag(TC_DES_ISO9797_algorithm algorithm,
                                                            TC_DES_ISO9797_padding padding,
                                                            bytes key, bytes message,
                                                            buffer tag) noexcept
{
  return TC_DES_ISO9797_MAC_short_tag(algorithm, padding, key, message, tag);
}
TC_CPP_NODISCARD inline TC_status des_iso9797_verify_short_tag(TC_DES_ISO9797_algorithm algorithm,
                                                               TC_DES_ISO9797_padding padding,
                                                               bytes key, bytes message,
                                                               bytes tag) noexcept
{
  return TC_DES_ISO9797_verify_short_tag(algorithm, padding, key, message, tag);
}

/* Streaming ISO/IEC 9797-1 MAC. init takes the algorithm, padding and key of
 * des_iso9797_mac. finish writes the full MAC and consumes the key. A failed
 * init, a finish and clear leave the object unkeyed, and update and finish
 * then return TC_ERROR until the next successful init. An update with invalid
 * arguments keeps the state.
 * Compare a received MAC with des_iso9797_verify or tiny_crypto::ct_equal.
 * The destructor clears the context. */
class DESISO9797 {
public:
  static const size_t tag_size = TC_DES_BLOCKLEN;

  DESISO9797() noexcept = default;
  ~DESISO9797() noexcept
  {
    TC_DES_ISO9797_ctx_clear(&ctx_);
  }
  DESISO9797(const DESISO9797&) = delete;
  DESISO9797& operator=(const DESISO9797&) = delete;

  TC_CPP_NODISCARD TC_status init(TC_DES_ISO9797_algorithm algorithm,
                                  TC_DES_ISO9797_padding padding, bytes key) noexcept
  {
    return TC_DES_ISO9797_init(&ctx_, algorithm, padding, key);
  }
  template <size_t N>
  TC_CPP_NODISCARD TC_status init(TC_DES_ISO9797_algorithm algorithm,
                                  TC_DES_ISO9797_padding padding, const uint8_t (&key)[N]) noexcept
  {
    return init(algorithm, padding, bytes{key, N});
  }
  TC_CPP_NODISCARD TC_status update(bytes message) noexcept
  {
    return TC_DES_ISO9797_update(&ctx_, message);
  }
  template <size_t N> TC_CPP_NODISCARD TC_status update(const uint8_t (&message)[N]) noexcept
  {
    return update(bytes{message, N});
  }
  TC_CPP_NODISCARD TC_status finish(uint8_t (&tag)[TC_DES_BLOCKLEN]) noexcept
  {
    return TC_DES_ISO9797_final(&ctx_, TC_buffer{tag, TC_DES_BLOCKLEN});
  }
  void clear() noexcept
  {
    TC_DES_ISO9797_ctx_clear(&ctx_);
  }

private:
  TC_DES_ISO9797_ctx ctx_{};
};
#endif

} /* namespace tiny_crypto */

#endif
#endif /* TINY_CRYPTO_DES_HPP_ */
