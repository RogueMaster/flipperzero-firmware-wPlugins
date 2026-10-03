/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_KDF_H_
#define TINY_CRYPTO_KDF_H_

#include <tiny_crypto/common.h>

/**
 * @file kdf.h
 * @brief NIST SP 800-108r1 key-based key derivation (KBKDF) in counter,
 *        feedback and double-pipeline mode over HMAC and CMAC PRFs.
 *
 * Every derivation is a one-shot: it expands a key-derivation key
 * (KDK) and caller-supplied fixed input into out.capacity bytes of keying material.
 * The fixed input is opaque to the library. TC_KBKDF_fixed_input builds
 * the conventional Label || 0x00 || Context || [L]_32 encoding. A cached
 * keyed PRF context, a working copy and chaining values live on the stack and
 * are wiped before return.
 * Standards: SP 800-108r1 sections 4.1 (counter), 4.2 (feedback) and 4.3
 * (double-pipeline). PRFs: FIPS 198-1 HMAC and SP 800-38B CMAC.
 * Work: every function charges no work budget. The functions return TC_OK
 * or TC_ERROR.
 * Library-wide contracts: docs/api.md.
 *
 * The header declares its API only when TC_ENABLE_KDF is 1. Function families
 * exist per PRF, each with _counter, _feedback and _pipeline variants, and
 * each also needs TC_ENABLE_KDF:
 *
 *   TC_KBKDF_HMAC_SHA1_*    TC_ENABLE_HMAC && TC_ENABLE_SHA1
 *   TC_KBKDF_HMAC_SHA224_*  TC_ENABLE_HMAC && TC_ENABLE_SHA224
 *   TC_KBKDF_HMAC_SHA256_*  TC_ENABLE_HMAC && TC_ENABLE_SHA256
 *   TC_KBKDF_HMAC_SHA384_*  TC_ENABLE_HMAC && TC_ENABLE_SHA384
 *   TC_KBKDF_HMAC_SHA512_*  TC_ENABLE_HMAC && TC_ENABLE_SHA512
 *   TC_KBKDF_AES_CMAC_*     TC_ENABLE_AES && TC_AES_ENABLE_CMAC (key = TC_AES_KEYLEN)
 *   TC_KBKDF_DES_CMAC_*     TC_ENABLE_DES && TC_DES_ENABLE_CMAC (legacy, 64-bit PRF)
 */

/* PRF availability, resolved once so kdf.c, kdf.hpp and tests share it. Each
 * macro is 0 when TC_ENABLE_KDF is 0. */
#define TC_KBKDF_HAVE_HMAC_SHA1 (TC_ENABLE_KDF && TC_ENABLE_HMAC && TC_ENABLE_SHA1)
#define TC_KBKDF_HAVE_HMAC_SHA224 (TC_ENABLE_KDF && TC_ENABLE_HMAC && TC_ENABLE_SHA224)
#define TC_KBKDF_HAVE_HMAC_SHA256 (TC_ENABLE_KDF && TC_ENABLE_HMAC && TC_ENABLE_SHA256)
#define TC_KBKDF_HAVE_HMAC_SHA384 (TC_ENABLE_KDF && TC_ENABLE_HMAC && TC_ENABLE_SHA384)
#define TC_KBKDF_HAVE_HMAC_SHA512 (TC_ENABLE_KDF && TC_ENABLE_HMAC && TC_ENABLE_SHA512)
#define TC_KBKDF_HAVE_AES_CMAC (TC_ENABLE_KDF && TC_ENABLE_AES && TC_AES_ENABLE_CMAC)
#define TC_KBKDF_HAVE_DES_CMAC (TC_ENABLE_KDF && TC_ENABLE_DES && TC_DES_ENABLE_CMAC)

#define TC_KBKDF_HAVE_HMAC                                                                         \
  (TC_KBKDF_HAVE_HMAC_SHA1 || TC_KBKDF_HAVE_HMAC_SHA224 || TC_KBKDF_HAVE_HMAC_SHA256 ||            \
   TC_KBKDF_HAVE_HMAC_SHA384 || TC_KBKDF_HAVE_HMAC_SHA512)

#if TC_ENABLE_KDF

#if TC_KBKDF_HAVE_HMAC
#include <tiny_crypto/hash.h>
#endif
#if TC_KBKDF_HAVE_AES_CMAC
#include <tiny_crypto/aes.h>
#endif
#if TC_KBKDF_HAVE_DES_CMAC
#include <tiny_crypto/des.h>
#endif

/* Largest PRF output h in bytes over the PRFs enabled in this profile, for
 * sizing caller buffers such as a feedback-mode IV. */
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

/* Counter width r in bits. SP 800-108r1 section 4 allows 8, 16, 24 or 32. */
#define TC_KBKDF_COUNTER_8 8
#define TC_KBKDF_COUNTER_16 16
#define TC_KBKDF_COUNTER_24 24
#define TC_KBKDF_COUNTER_32 32

/*
 * Counter placement for feedback and double-pipeline mode (the CAVP
 * CTRLOCATION values). "iter" is K(i-1) in feedback mode and A(i) in
 * double-pipeline mode. Values start at 1 so a zero-initialized params struct
 * with use_counter set is rejected.
 */
#define TC_KBKDF_CTR_BEFORE_ITER 1 /**< [i]_r || iter || FixedInput */
#define TC_KBKDF_CTR_AFTER_ITER 2  /**< iter || [i]_r || FixedInput */
#define TC_KBKDF_CTR_AFTER_FIXED 3 /**< iter || FixedInput || [i]_r */

/**
 * @brief Derivation parameters shared by every PRF family.
 *
 * Counter mode reads only counter_bits. The counter position is expressed by
 * the before/after split of the fixed input (see the *_counter contract).
 * Feedback and double-pipeline mode read use_counter and, when it is
 * non-zero, counter_bits and counter_location as well. Ignored fields may
 * hold any value.
 */
struct TC_KBKDF_params {
  uint8_t counter_bits;     /**< TC_KBKDF_COUNTER_8 / 16 / 24 / 32 */
  uint8_t counter_location; /**< TC_KBKDF_CTR_* (feedback / pipeline with counter) */
  uint8_t use_counter;      /**< 0 or 1 (feedback / pipeline only) */
};

/* Exact size of the buffer TC_KBKDF_fixed_input writes. */
#define TC_KBKDF_FIXED_INPUT_LEN(label_len, context_len)                                           \
  ((size_t)(label_len) + 1u + (size_t)(context_len) + 4u)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the conventional fixed input Label || 0x00 || Context || [8*out_len]_32.
 *
 * Binds the purpose (label), the parties or session (context) and the
 * requested length in bits, big-endian, as SP 800-108r1 section 4 recommends.
 * The 0x00 separator only delimits unambiguously when label contains no zero
 * byte, so such labels are rejected.
 * @param label Label bytes in the application's encoding (typically ASCII).
 * @param context Context bytes.
 * @param out_len Byte length of the keying material the caller will derive
 *                with this fixed input, 1..2^29 - 1 so that 8 * out_len fits
 *                in 32 bits.
 * @param output Output storage, disjoint from label and context. Its capacity
 *               must be at least TC_KBKDF_FIXED_INPUT_LEN(label.length,
 *               context.length). Exactly that many bytes are written. Pass
 *               that length as the fixed input to the derivation.
 * @return TC_OK, or TC_ERROR with output unchanged for a NULL output.data, a
 *         span with NULL data and a nonzero length, an out_len outside the
 *         range, a short capacity, an overlapping output, or a zero byte
 *         inside label.
 */
TC_status TC_KBKDF_fixed_input(TC_bytes label, TC_bytes context, size_t out_len, TC_buffer output);

/*
 * Common contract for every TC_KBKDF_<PRF>_<mode> function below.
 *
 * key             The KDK. key.data must be non-NULL and key.length non-zero.
 *                 AES-CMAC requires key.length == TC_AES_KEYLEN (the key size
 *                 is fixed by TC_AES_KEY_BITS). DES-CMAC accepts 8, 16 (2-key
 *                 TDEA, K1 || K2 used as K1, K2, K1) or 24. HMAC accepts any
 *                 non-zero length. Keys longer than a block are hashed.
 * params          Non-NULL. counter_bits must be 8, 16, 24 or 32 whenever a
 *                 counter is used. counter_location must be a TC_KBKDF_CTR_*
 *                 value for feedback / pipeline mode with a counter.
 * inputs          A span may have NULL data only when its length is 0.
 *                 An empty fixed input and an empty IV are valid.
 * out             Exactly out.capacity bytes are written. The capacity must be
 *                 non-zero, and out must not overlap the key, the IV or any
 *                 fixed-input span (TC_ERROR otherwise, because later blocks
 *                 re-read the inputs). n = ceil(out.capacity / h) PRF blocks
 *                 are computed and the last one is truncated. n must not
 *                 exceed 2^r - 1 when a counter of r bits is used, nor
 *                 2^32 - 1 without one.
 * Return          TC_OK, or TC_ERROR. A violation of the rules above leaves
 *                 out unchanged. So does a PRF key init failure, such as a
 *                 DES key refused under TC_DES_REJECT_WEAK_KEYS. A PRF
 *                 failure after derivation starts wipes out.
 *
 * Counter mode (SP 800-108r1 section 4.1):
 *   K(i) = PRF(KDK, before || [i]_r || after), i = 1..n
 *   before empty  ->  CAVP BEFORE_FIXED  ([i] || FixedInput)
 *   after  empty  ->  CAVP AFTER_FIXED   (FixedInput || [i])
 *   both present  ->  CAVP MIDDLE_FIXED  (DataBeforeCtr || [i] || DataAfterCtr)
 *
 * Feedback mode (section 4.2):
 *   K(0) = iv (may be empty)
 *   K(i) = PRF(KDK, K(i-1) [|| [i]_r] || fixed), counter placed per
 *          params->counter_location when params->use_counter is non-zero
 *
 * Double-pipeline mode (section 4.3):
 *   A(0) = fixed, A(i) = PRF(KDK, A(i-1))
 *   K(i) = PRF(KDK, A(i) [|| [i]_r] || fixed), counter as in feedback mode
 *
 * The streaming PRF APIs always yield the full h-byte block, so
 * TC_HMAC_MIN_TAG_LEN and TC_MIN_TAG_LEN do not apply here.
 *
 * CMAC key control: with a CMAC PRF, a party that knows the KDK and chooses
 * part of the fixed input or IV can steer output blocks (SP 800-108r1 section
 * 6.7 and Appendix B). When that matters, prefer an HMAC PRF or apply the
 * mitigations of sections 4.1-4.3. Counter mode appends
 * K(0) = PRF(KDK, fixed input) to the fixed input. Feedback mode uses that
 * K(0) as the IV and a counter. Double-pipeline mode uses a counter. K(0)
 * is the feedback-mode output for an empty IV, no counter and an out.capacity
 * of h bytes.
 */

#if TC_KBKDF_HAVE_HMAC_SHA1
/** @brief KBKDF counter mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                     TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                      TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-1 (h = 20). */
TC_status TC_KBKDF_HMAC_SHA1_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                      TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_HMAC_SHA1 */

#if TC_KBKDF_HAVE_HMAC_SHA224
/** @brief KBKDF counter mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                       TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-224 (h = 28). */
TC_status TC_KBKDF_HMAC_SHA224_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_HMAC_SHA224 */

#if TC_KBKDF_HAVE_HMAC_SHA256
/** @brief KBKDF counter mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                       TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-256 (h = 32). */
TC_status TC_KBKDF_HMAC_SHA256_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_HMAC_SHA256 */

#if TC_KBKDF_HAVE_HMAC_SHA384
/** @brief KBKDF counter mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                       TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-384 (h = 48). */
TC_status TC_KBKDF_HMAC_SHA384_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_HMAC_SHA384 */

#if TC_KBKDF_HAVE_HMAC_SHA512
/** @brief KBKDF counter mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                       TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with HMAC-SHA-512 (h = 64). */
TC_status TC_KBKDF_HMAC_SHA512_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                        TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_HMAC_SHA512 */

#if TC_KBKDF_HAVE_AES_CMAC
/** @brief KBKDF counter mode with AES-CMAC (h = 16, key.length must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                    TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with AES-CMAC (h = 16, key.length must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                     TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with AES-CMAC (h = 16, key.length must be TC_AES_KEYLEN). */
TC_status TC_KBKDF_AES_CMAC_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                     TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_AES_CMAC */

#if TC_KBKDF_HAVE_DES_CMAC
/*
 * TDEA-CMAC is a 64-bit-block PRF kept for CAVP and legacy interoperability
 * (SP 800-131A deprecates it). key.length is 8, 16 or 24.
 */
/** @brief KBKDF counter mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_counter(TC_bytes key, const struct TC_KBKDF_params* params,
                                    TC_bytes before, TC_bytes after, TC_buffer out);
/** @brief KBKDF feedback mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_feedback(TC_bytes key, const struct TC_KBKDF_params* params,
                                     TC_bytes iv, TC_bytes fixed, TC_buffer out);
/** @brief KBKDF double-pipeline mode with DES/TDEA-CMAC (h = 8). */
TC_status TC_KBKDF_DES_CMAC_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,
                                     TC_bytes fixed, TC_buffer out);
#endif /* TC_KBKDF_HAVE_DES_CMAC */

#ifdef __cplusplus
}
#endif

#endif /* TC_ENABLE_KDF */
#endif /* TINY_CRYPTO_KDF_H_ */
