/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_KDF_HPP_
#define TINY_CRYPTO_KDF_HPP_

#ifndef __cplusplus
#error Do not include kdf.hpp in a C project, include kdf.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/kdf.h>

namespace tiny_crypto {

/* Plain aggregate; brace-initialize as {counter_bits, counter_location,
 * use_counter}, e.g. kbkdf_params{TC_KBKDF_COUNTER_32, 0, 0} for counter mode. */
typedef ::TC_KBKDF_params kbkdf_params;

/* Label || 0x00 || Context || [8 * out_len]_32; see TC_KBKDF_fixed_input. */
inline TC_status kbkdf_fixed_input(const uint8_t* label, size_t label_len,
                                   const uint8_t* context, size_t context_len,
                                   size_t out_len, uint8_t* buf,
                                   size_t buf_len) {
    return TC_KBKDF_fixed_input(label, label_len, context, context_len, out_len,
                                buf, buf_len);
}

/*
 * One family per PRF. Status values and contracts come directly from kdf.h.
 * The macro is file-local and undefined at the end of this header.
 */
#define TINY_CRYPTO_KBKDF_FAMILY(cpp_name, C_NAME) \
    inline TC_status cpp_name##_counter(const uint8_t* key, size_t key_len, \
                                   const kbkdf_params& params, \
                                   const uint8_t* before, size_t before_len, \
                                   const uint8_t* after, size_t after_len, \
                                   uint8_t* out, size_t out_len) { \
        return TC_KBKDF_##C_NAME##_counter(key, key_len, &params, before, before_len, \
                                           after, after_len, out, out_len); \
    } \
    inline TC_status cpp_name##_feedback(const uint8_t* key, size_t key_len, \
                                    const kbkdf_params& params, \
                                    const uint8_t* iv, size_t iv_len, \
                                    const uint8_t* fixed, size_t fixed_len, \
                                    uint8_t* out, size_t out_len) { \
        return TC_KBKDF_##C_NAME##_feedback(key, key_len, &params, iv, iv_len, fixed, \
                                            fixed_len, out, out_len); \
    } \
    inline TC_status cpp_name##_pipeline(const uint8_t* key, size_t key_len, \
                                    const kbkdf_params& params, \
                                    const uint8_t* fixed, size_t fixed_len, \
                                    uint8_t* out, size_t out_len) { \
        return TC_KBKDF_##C_NAME##_pipeline(key, key_len, &params, fixed, fixed_len, \
                                            out, out_len); \
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
/* The AES key size is fixed per build; a wrong key_len returns TC_ERROR. */
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_aes_cmac, AES_CMAC)
#endif
#if TC_KBKDF_HAVE_DES_CMAC
TINY_CRYPTO_KBKDF_FAMILY(kbkdf_des_cmac, DES_CMAC)
#endif

#undef TINY_CRYPTO_KBKDF_FAMILY

} // namespace tiny_crypto

#endif /* TINY_CRYPTO_KDF_HPP_ */
