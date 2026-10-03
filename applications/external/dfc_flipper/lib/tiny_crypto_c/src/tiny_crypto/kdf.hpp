/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* KBKDF wrappers for kdf.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_KDF_HPP_
#define TINY_CRYPTO_KDF_HPP_

#ifndef __cplusplus
#error Do not include kdf.hpp in a C project, include kdf.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/kdf.h>

#if TC_ENABLE_KDF
namespace tiny_crypto {

/* Plain aggregate. Brace-initialize as {counter_bits, counter_location,
 * use_counter}, e.g. kbkdf_params{TC_KBKDF_COUNTER_32, 0, 0} for counter mode. */
typedef ::TC_KBKDF_params kbkdf_params;

/* Label || 0x00 || Context || [8 * out_len]_32. See TC_KBKDF_fixed_input. */
TC_CPP_NODISCARD inline TC_status kbkdf_fixed_input(bytes label, bytes context, size_t out_len,
                                                    buffer output) noexcept
{
  return TC_KBKDF_fixed_input(label, context, out_len, output);
}

/*
 * One family per PRF. Status values and contracts come directly from kdf.h.
 * The macro is file-local and undefined at the end of this header.
 */
#define TINY_CRYPTO_KBKDF_FAMILY(cpp_name, C_NAME)                                                 \
  TC_CPP_NODISCARD inline TC_status cpp_name##_counter(                                            \
      bytes key, const kbkdf_params& params, bytes before, bytes after, buffer out) noexcept       \
  {                                                                                                \
    return TC_KBKDF_##C_NAME##_counter(key, &params, before, after, out);                          \
  }                                                                                                \
  TC_CPP_NODISCARD inline TC_status cpp_name##_feedback(                                           \
      bytes key, const kbkdf_params& params, bytes iv, bytes fixed, buffer out) noexcept           \
  {                                                                                                \
    return TC_KBKDF_##C_NAME##_feedback(key, &params, iv, fixed, out);                             \
  }                                                                                                \
  TC_CPP_NODISCARD inline TC_status cpp_name##_pipeline(bytes key, const kbkdf_params& params,     \
                                                        bytes fixed, buffer out) noexcept          \
  {                                                                                                \
    return TC_KBKDF_##C_NAME##_pipeline(key, &params, fixed, out);                                 \
  }

#if TC_KBKDF_HAVE_HMAC_SHA1
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_hmac_sha1, HMAC_SHA1)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_hmac_sha224, HMAC_SHA224)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_hmac_sha256, HMAC_SHA256)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_hmac_sha384, HMAC_SHA384)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_hmac_sha512, HMAC_SHA512)
#endif
#if TC_KBKDF_HAVE_AES_CMAC
/* The AES key size is fixed per build. A wrong key length returns TC_ERROR. */
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_aes_cmac, AES_CMAC)
#endif
#if TC_KBKDF_HAVE_DES_CMAC
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_des_cmac, DES_CMAC)
#endif

#undef TINY_CRYPTO_KBKDF_FAMILY

} // namespace tiny_crypto
#endif /* TC_ENABLE_KDF */

#endif /* TINY_CRYPTO_KDF_HPP_ */
