/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* One-step key derivation with a hash function over each enabled SHA.
 * Standards: SP 800-56C Rev. 2 section 4.1, option 1.
 * Configuration: TC_ENABLE_SSKDF with TC_ENABLE_SHA*.
 * Limitations: the HMAC and KMAC auxiliary functions of section 4.1 are
 * unsupported.
 * Work: every function charges no work budget.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_SSKDF_H_
#define TINY_CRYPTO_SSKDF_H_
#include <tiny_crypto/common.h>

#if TC_ENABLE_SSKDF
#ifdef __cplusplus
extern "C" {
#endif

/* SP 800-56C Rev. 2 section 4.1 one-step KDF with H(x) = hash(x) (Option 1).
 * Block i is hash([i]_32 || Z || FixedInfo) for i = 1..n, and the output is
 * the leftmost output.capacity bytes of the concatenated blocks. One function
 * exists per SHA enabled in the profile. Section 4.2 Table 1 approves each
 * of these hashes.
 *
 * z               Shared secret. Must be nonempty.
 * info, count     FixedInfo as the in-order concatenation of count spans.
 *                 The span array is read in place and may be NULL when count
 *                 is 0. A span may have NULL data only when its length is 0.
 * output          Exactly output.capacity bytes are written. The capacity
 *                 must be nonzero and at most (2^32 - 1) digests (section 4.1
 *                 step 2). output must be disjoint from z, every info span
 *                 and the span array.
 *
 * Returns TC_OK, or TC_ERROR with output unchanged for a NULL or empty z, a
 * NULL output.data, a zero or oversized capacity, a NULL info with a nonzero
 * count, an invalid span, an overlap, or a hash input above 2^64 - 1 bits
 * (section 4.1 step 4). A hash failure after derivation starts wipes output.
 * The hash context and digest block live on the stack and are wiped before
 * return. All lengths are bytes. */
#if TC_ENABLE_SHA1
TC_status TC_SSKDF_SHA1(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output);
#endif
#if TC_ENABLE_SHA224
TC_status TC_SSKDF_SHA224(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output);
#endif
#if TC_ENABLE_SHA256
TC_status TC_SSKDF_SHA256(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output);
#endif
#if TC_ENABLE_SHA384
TC_status TC_SSKDF_SHA384(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output);
#endif
#if TC_ENABLE_SHA512
TC_status TC_SSKDF_SHA512(TC_bytes z, const TC_bytes* info, size_t count, TC_buffer output);
#endif

#ifdef __cplusplus
}
#endif
#endif /* TC_ENABLE_SSKDF */
#endif
