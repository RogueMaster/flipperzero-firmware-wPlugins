/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* HKDF extract, expand and one-shot derive over each enabled HMAC-SHA family.
 * Standards: RFC 5869, SP 800-56C Rev. 2 section 5.
 * Configuration: TC_ENABLE_HKDF with TC_ENABLE_HMAC and TC_ENABLE_SHA*.
 * Work: every function charges no work budget. The functions return TC_OK
 * or TC_ERROR.
 * Contracts: docs/api.md. Guide: docs/hkdf.md. */
#ifndef TINY_CRYPTO_HKDF_H_
#define TINY_CRYPTO_HKDF_H_

#include <tiny_crypto/common.h>

#if TC_ENABLE_HKDF
#include <tiny_crypto/hash.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RFC 5869 HKDF over each enabled HMAC-SHA family.
 *
 * extract (RFC 5869 section 2.2) computes PRK = HMAC(salt, IKM) and writes
 * exactly TC_SHA*_DIGESTLEN bytes to prk. The input keying material is the
 * concatenation of ikm_count spans, read in order without copying. ikm may
 * be NULL when ikm_count is 0. Pass one span for an ordinary secret, or Z
 * and T for an SP 800-56C Rev. 2 hybrid secret Z || T. An empty salt is the
 * RFC's all-zero salt.
 *
 * expand (section 2.3) accepts a PRK at least one digest long and writes
 * exactly output.capacity bytes, 1..255 * HashLen. derive runs extract then
 * expand and wipes its PRK. Every HMAC context and chaining block lives on
 * the stack and is wiped before return.
 *
 * A span may have NULL data only when it is empty. Output must be disjoint
 * from every input, including the ikm array. Each function returns TC_OK, or
 * TC_ERROR with output unchanged for a NULL output, an invalid span, a NULL
 * ikm with a nonzero ikm_count, an ikm_count whose array size overflows
 * size_t, an overlap, a short or NULL PRK (expand) or an output.capacity
 * outside 1..255 * HashLen. A failure after processing begins wipes
 * output. */
#define TC_HKDF_DECLARE(N)                                                                         \
  TC_status TC_HKDF_SHA##N##_extract(TC_bytes salt, const TC_bytes* ikm, size_t ikm_count,         \
                                     uint8_t prk[TC_SHA##N##_DIGESTLEN]);                          \
  TC_status TC_HKDF_SHA##N##_expand(TC_bytes prk, TC_bytes info, TC_buffer output);                \
  TC_status TC_HKDF_SHA##N##_derive(TC_bytes salt, const TC_bytes* ikm, size_t ikm_count,          \
                                    TC_bytes info, TC_buffer output)

#if TC_ENABLE_SHA1
TC_HKDF_DECLARE(1);
#endif
#if TC_ENABLE_SHA224
TC_HKDF_DECLARE(224);
#endif
#if TC_ENABLE_SHA256
TC_HKDF_DECLARE(256);
#endif
#if TC_ENABLE_SHA384
TC_HKDF_DECLARE(384);
#endif
#if TC_ENABLE_SHA512
TC_HKDF_DECLARE(512);
#endif

#undef TC_HKDF_DECLARE

#ifdef __cplusplus
}
#endif
#endif /* TC_ENABLE_HKDF */
#endif /* TINY_CRYPTO_HKDF_H_ */
