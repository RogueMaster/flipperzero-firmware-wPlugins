/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_KDF_H_
#define TINY_CRYPTO_KDF_H_

#include <tiny_crypto/common.h>

/**
 * @file kdf.h
 * @brief NIST SP 800-108 key-based key derivation (KBKDF) in counter,
 *        feedback and double-pipeline mode over HMAC and CMAC PRFs.
 *
 * Every derivation is a heap-free one-shot: it expands a key-derivation key
 * (KDK) and caller-supplied fixed input into out_len bytes of keying material.
 * The library never interprets the fixed input; TC_KBKDF_fixed_input builds
 * the conventional Label || 0x00 || Context || [L]_32 encoding. A cached
 * keyed PRF context, a working copy and chaining values live on the stack and
 * are wiped when TC_ZEROIZE is 1.
 *
 * Function families exist per PRF, each with _counter, _feedback and
 * _pipeline variants:
 *
 *   TC_KBKDF_HMAC_SHA1_*    TC_ENABLE_HMAC && TC_ENABLE_SHA1
 *   TC_KBKDF_HMAC_SHA224_*  TC_ENABLE_HMAC && TC_ENABLE_SHA224
 *   TC_KBKDF_HMAC_SHA256_*  TC_ENABLE_HMAC && TC_ENABLE_SHA256
 *   TC_KBKDF_HMAC_SHA384_*  TC_ENABLE_HMAC && TC_ENABLE_SHA384
 *   TC_KBKDF_HMAC_SHA512_*  TC_ENABLE_HMAC && TC_ENABLE_SHA512
 *   TC_KBKDF_AES_CMAC_*     TC_ENABLE_AES && TC_AES_ENABLE_CMAC (key = TC_AES_KEYLEN)
 *   TC_KBKDF_DES_CMAC_*     TC_ENABLE_DES && TC_DES_ENABLE_CMAC (legacy, 64-bit PRF)
 */

#if (TC_ENABLE_KDF != 0) && (TC_ENABLE_KDF != 1)
  #error "TC_ENABLE_KDF must be 0 or 1"
#endif

/* PRF availability, resolved once so kdf.c, kdf.hpp and tests share it. */
#define TC_KBKDF_HAVE_HMAC_SHA1   (TC_ENABLE_HMAC && TC_ENABLE_SHA1)
#define TC_KBKDF_HAVE_HMAC_SHA224 (TC_ENABLE_HMAC && TC_ENABLE_SHA224)
#define TC_KBKDF_HAVE_HMAC_SHA256 (TC_ENABLE_HMAC && TC_ENABLE_SHA256)
#define TC_KBKDF_HAVE_HMAC_SHA384 (TC_ENABLE_HMAC && TC_ENABLE_SHA384)
#define TC_KBKDF_HAVE_HMAC_SHA512 (TC_ENABLE_HMAC && TC_ENABLE_SHA512)
#define TC_KBKDF_HAVE_AES_CMAC    (TC_ENABLE_AES && TC_AES_ENABLE_CMAC)
#define TC_KBKDF_HAVE_DES_CMAC    (TC_ENABLE_DES && TC_DES_ENABLE_CMAC)

#define TC_KBKDF_HAVE_HMAC \
  (TC_KBKDF_HAVE_HMAC_SHA1 || TC_KBKDF_HAVE_HMAC_SHA224 || TC_KBKDF_HAVE_HMAC_SHA256 || \
   TC_KBKDF_HAVE_HMAC_SHA384 || TC_KBKDF_HAVE_HMAC_SHA512)

#if TC_ENABLE_KDF && \
    !(TC_KBKDF_HAVE_HMAC || TC_KBKDF_HAVE_AES_CMAC || TC_KBKDF_HAVE_DES_CMAC)
  #error "TC_ENABLE_KDF needs a PRF: HMAC with an enabled SHA, TC_AES_ENABLE_CMAC or TC_DES_ENABLE_CMAC"
#endif

#if TC_KBKDF_HAVE_HMAC
#include <tiny_crypto/hash.h>
#endif
#if TC_KBKDF_HAVE_AES_CMAC
#include <tiny_crypto/aes.h>
#endif
#if TC_KBKDF_HAVE_DES_CMAC
#include <tiny_crypto/des.h>
#endif

/*
 * Largest PRF output (h) in this profile. Sizes the feedback / pipeline
 * chaining buffers so small profiles do not pay for SHA-512.
 */
#if TC_KBKDF_HAVE_HMAC_SHA512
  #define TC_KBKDF_PRF_MAX 64
#elif TC_KBKDF_HAVE_HMAC_SHA384
  #define TC_KBKDF_PRF_MAX 48
#elif TC_KBKDF_HAVE_HMAC_SHA256
  #define TC_KBKDF_PRF_MAX 32
#elif TC_KBKDF_HAVE_HMAC_SHA224
  #define TC_KBKDF_PRF_MAX 28
#elif TC_KBKDF_HAVE_HMAC_SHA1
  #define TC_KBKDF_PRF_MAX 20
#elif TC_KBKDF_HAVE_AES_CMAC
  #define TC_KBKDF_PRF_MAX 16
#else
  #define TC_KBKDF_PRF_MAX 8
#endif

/* Counter width r in bits. SP 800-108 allows 8, 16, 24 or 32. */
#define TC_KBKDF_COUNTER_8  8
#define TC_KBKDF_COUNTER_16 16
#define TC_KBKDF_COUNTER_24 24
#define TC_KBKDF_COUNTER_32 32

/*
 * Counter placement for feedback and double-pipeline mode (the CAVP
 * CTRLOCATION values). "iter" is K(i-1) in feedback mode and A(i) in
 * double-pipeline mode. Values start at 1 so a zero-initialized params struct
 * with use_counter set is rejected instead of silently picking a layout.
 */
#define TC_KBKDF_CTR_BEFORE_ITER 1 /**< [i]_r || iter || FixedInput */
#define TC_KBKDF_CTR_AFTER_ITER  2 /**< iter || [i]_r || FixedInput */
#define TC_KBKDF_CTR_AFTER_FIXED 3 /**< iter || FixedInput || [i]_r */

/**
 * @brief Derivation parameters shared by every PRF family.
 *
 * Counter mode reads only counter_bits; the counter position is expressed by
 * the before/after split of the fixed input (see the *_counter contract).
 * Feedback and double-pipeline mode read use_counter and, when it is
 * non-zero, counter_bits and counter_location as well. Ignored fields may
 * hold any value.
 */
struct TC_KBKDF_params
{
  uint8_t counter_bits;     /**< TC_KBKDF_COUNTER_8 / 16 / 24 / 32 */
  uint8_t counter_location; /**< TC_KBKDF_CTR_* (feedback / pipeline with counter) */
  uint8_t use_counter;      /**< 0 or 1 (feedback / pipeline only) */
};

/* Exact size of the buffer TC_KBKDF_fixed_input writes. */
#define TC_KBKDF_FIXED_INPUT_LEN(label_len, context_len) \
  ((size_t)(label_len) + 1u + (size_t)(context_len) + 4u)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the conventional fixed input Label || 0x00 || Context || [8*out_len]_32.
 *
 * Binds the purpose (label), the parties or session (context) and the
 * requested length in bits, big-endian, as SP 800-108 section 4 recommends.
 * The 0x00 separator only delimits unambiguously when label contains no zero
 * byte, so such labels are rejected.
 * @param label Label bytes in the application's encoding (typically ASCII);
 *              may be NULL when label_len is 0.
 * @param context Context bytes; may be NULL when context_len is 0.
 * @param out_len Byte length of the keying material the caller will derive
 *                with this fixed input; 1..2^29 - 1 so that 8 * out_len fits
 *                in 32 bits.
 * @param buf Output buffer; must not overlap label or context.
 * @param buf_len Capacity of buf; at least
 *                TC_KBKDF_FIXED_INPUT_LEN(label_len, context_len). Exactly that
 *                many bytes are written; pass that value as the fixed-input
 *                length to the derivation.
 * @return TC_OK, or TC_ERROR on a NULL/length violation, an overlapping buf,
 *         or a zero byte inside label.
 */
TC_status TC_KBKDF_fixed_input(const uint8_t* label, size_t label_len,
                               const uint8_t* context, size_t context_len,
                               size_t out_len, uint8_t* buf, size_t buf_len);

/*
 * Common contract for every TC_KBKDF_<PRF>_<mode> function below.
 *
 * key / key_len   The KDK. key must be non-NULL and key_len non-zero.
 *                 AES-CMAC requires key_len == TC_AES_KEYLEN (the key size is
 *                 fixed by TC_AES_KEY_BITS). DES-CMAC accepts 8, 16 (2-key
 *                 TDEA, K1 || K2 used as K1, K2, K1) or 24. HMAC accepts any
 *                 non-zero length; keys longer than a block are hashed.
 * params          Non-NULL. counter_bits must be 8, 16, 24 or 32 whenever a
 *                 counter is used. counter_location must be a TC_KBKDF_CTR_*
 *                 value for feedback / pipeline mode with a counter.
 * inputs          Any input pointer may be NULL only when its length is 0.
 *                 An empty fixed input and an empty IV are valid.
 * out / out_len   Exactly out_len bytes are written. out_len must be non-zero
 *                 and out must not overlap key, the IV or any fixed-input
 *                 buffer (TC_ERROR otherwise; later blocks re-read the inputs).
 *                 n = ceil(out_len / h) PRF blocks are computed and the last
 *                 one is truncated. n must not exceed 2^r - 1 when a counter of
 *                 r bits is used, nor 2^32 - 1 without one.
 * Return          TC_OK, or TC_ERROR on any violation. out is wiped if the
 *                 error is detected after derivation started.
 *
 * Counter mode (SP 800-108 section 5.1):
 *   K(i) = PRF(KDK, before || [i]_r || after), i = 1..n
 *   before empty  ->  CAVP BEFORE_FIXED  ([i] || FixedInput)
 *   after  empty  ->  CAVP AFTER_FIXED   (FixedInput || [i])
 *   both present  ->  CAVP MIDDLE_FIXED  (DataBeforeCtr || [i] || DataAfterCtr)
 *
 * Feedback mode (section 5.2):
 *   K(0) = iv (may be empty)
 *   K(i) = PRF(KDK, K(i-1) [|| [i]_r] || fixed), counter placed per
 *          params->counter_location when params->use_counter is non-zero
 *
 * Double-pipeline mode (section 5.3):
 *   A(0) = fixed, A(i) = PRF(KDK, A(i-1))
 *   K(i) = PRF(KDK, A(i) [|| [i]_r] || fixed), counter as in feedback mode
 *
 * The streaming PRF APIs always yield the full h-byte block, so
 * TC_HMAC_MIN_TAG_LEN and the CMAC minimum tag lengths do not apply here.
 */

#if TC_KBKDF_HAVE_HMAC_SHA1
/** @brief KBKDF counter mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_counter(const uint8_t* key, size_t key_len,
                                     const struct TC_KBKDF_params* params,
                                     const uint8_t* before, size_t before_len,
                                     const uint8_t* after, size_t after_len,
                                     uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_feedback(const uint8_t* key, size_t key_len,
                                      const struct TC_KBKDF_params* params,
                                      const uint8_t* iv, size_t iv_len,
                                      const uint8_t* fixed, size_t fixed_len,
                                      uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_pipeline(const uint8_t* key, size_t key_len,
                                      const struct TC_KBKDF_params* params,
                                      const uint8_t* fixed, size_t fixed_len,
                                      uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_HMAC_SHA1 */

#if TC_KBKDF_HAVE_HMAC_SHA224
/** @brief KBKDF counter mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_counter(const uint8_t* key, size_t key_len,
                                       const struct TC_KBKDF_params* params,
                                       const uint8_t* before, size_t before_len,
                                       const uint8_t* after, size_t after_len,
                                       uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_feedback(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_pipeline(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_HMAC_SHA224 */

#if TC_KBKDF_HAVE_HMAC_SHA256
/** @brief KBKDF counter mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_counter(const uint8_t* key, size_t key_len,
                                       const struct TC_KBKDF_params* params,
                                       const uint8_t* before, size_t before_len,
                                       const uint8_t* after, size_t after_len,
                                       uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_feedback(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_pipeline(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_HMAC_SHA256 */

#if TC_KBKDF_HAVE_HMAC_SHA384
/** @brief KBKDF counter mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_counter(const uint8_t* key, size_t key_len,
                                       const struct TC_KBKDF_params* params,
                                       const uint8_t* before, size_t before_len,
                                       const uint8_t* after, size_t after_len,
                                       uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_feedback(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_pipeline(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_HMAC_SHA384 */

#if TC_KBKDF_HAVE_HMAC_SHA512
/** @brief KBKDF counter mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_counter(const uint8_t* key, size_t key_len,
                                       const struct TC_KBKDF_params* params,
                                       const uint8_t* before, size_t before_len,
                                       const uint8_t* after, size_t after_len,
                                       uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_feedback(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_pipeline(const uint8_t* key, size_t key_len,
                                        const struct TC_KBKDF_params* params,
                                        const uint8_t* fixed, size_t fixed_len,
                                        uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_HMAC_SHA512 */

#if TC_KBKDF_HAVE_AES_CMAC
/** @brief KBKDF counter mode with AES-CMAC (h = 16; key_len must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_counter(const uint8_t* key, size_t key_len,
                                    const struct TC_KBKDF_params* params,
                                    const uint8_t* before, size_t before_len,
                                    const uint8_t* after, size_t after_len,
                                    uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with AES-CMAC (h = 16; key_len must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_feedback(const uint8_t* key, size_t key_len,
                                     const struct TC_KBKDF_params* params,
                                     const uint8_t* iv, size_t iv_len,
                                     const uint8_t* fixed, size_t fixed_len,
                                     uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with AES-CMAC (h = 16; key_len must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_pipeline(const uint8_t* key, size_t key_len,
                                     const struct TC_KBKDF_params* params,
                                     const uint8_t* fixed, size_t fixed_len,
                                     uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_AES_CMAC */

#if TC_KBKDF_HAVE_DES_CMAC
/*
 * TDEA-CMAC is a 64-bit-block PRF kept for CAVP and legacy interoperability
 * (SP 800-131A deprecates it); key_len is 8, 16 or 24.
 */
/** @brief KBKDF counter mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_counter(const uint8_t* key, size_t key_len,
                                    const struct TC_KBKDF_params* params,
                                    const uint8_t* before, size_t before_len,
                                    const uint8_t* after, size_t after_len,
                                    uint8_t* out, size_t out_len);
/** @brief KBKDF feedback mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_feedback(const uint8_t* key, size_t key_len,
                                     const struct TC_KBKDF_params* params,
                                     const uint8_t* iv, size_t iv_len,
                                     const uint8_t* fixed, size_t fixed_len,
                                     uint8_t* out, size_t out_len);
/** @brief KBKDF double-pipeline mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_pipeline(const uint8_t* key, size_t key_len,
                                     const struct TC_KBKDF_params* params,
                                     const uint8_t* fixed, size_t fixed_len,
                                     uint8_t* out, size_t out_len);
#endif /* TC_KBKDF_HAVE_DES_CMAC */

#ifdef __cplusplus
}
#endif

#endif /* TINY_CRYPTO_KDF_H_ */
