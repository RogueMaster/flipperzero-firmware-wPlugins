/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* AES key wrap: one-shot KW and KWP wrap and unwrap under a key-encryption
 * key (KEK).
 * Standards: SP 800-38F sections 5 and 6 (KW-AE, KW-AD, KWP-AE, KWP-AD),
 * RFC 3394 (KW), RFC 5649 (KWP).
 * Configuration: TC_ENABLE_AES and TC_AES_ENABLE_KW. The KEK holds
 * TC_AES_KEYLEN bytes, or 16, 24 or 32 bytes when TC_AES_ENABLE_DYNAMIC is 1.
 * Limitations: default ICVs only. The AES forward cipher is the designated
 * cipher function. KWP key data is limited to 2^32 - 8 bytes. TDEA key wrap
 * (TKW) is absent.
 * Work: every function charges no work budget. The functions return
 * TC_status (TC_OK, TC_MISMATCH or TC_ERROR).
 * Contracts: docs/api.md and docs/aes-kw.md. */
#ifndef TINY_CRYPTO_AES_KW_H_
#define TINY_CRYPTO_AES_KW_H_
#include <tiny_crypto/aes.h>
#ifdef __cplusplus
extern "C" {
#endif

#if TC_AES_ENABLE_KW

/* KEK lengths in bytes that this build accepts. Fixed-key builds accept
 * TC_AES_KEYLEN only. Dynamic-key builds accept all three AES key sizes. */
#if TC_AES_ENABLE_DYNAMIC
#define TC_AES_KW_KEK_LENGTH_SUPPORTED(n) ((n) == 16u || (n) == 24u || (n) == 32u)
#else
#define TC_AES_KW_KEK_LENGTH_SUPPORTED(n) ((n) == TC_AES_KEYLEN)
#endif

/* Buffer sizes in bytes. The macros hold for lengths the functions accept
 * and use unchecked size_t arithmetic.
 * Wrapped size of n bytes of KW key data (n a multiple of 8, at least 16). */
#define TC_AES_KW_WRAPPED_BYTES(n) ((n) + 8u)
/* Wrapped size of n bytes of KWP key data (n at least 1): n rounded up to a
 * multiple of 8, plus 8. */
#define TC_AES_KWP_WRAPPED_BYTES(n) ((((n) + 7u) / 8u) * 8u + 8u)
/* Unwrap output capacity for w wrapped bytes (w at least 16): the exact KW
 * key data size and the largest KWP key data size. It wraps below 8. */
#define TC_AES_KW_UNWRAPPED_BYTES(w) ((w) - 8u)

/* Length limits in bytes. SP 800-38F section 5.3.1 Table 1 gives the KW
 * limits. The KWP limits follow the section 5.3.2 restriction, so every
 * KWP-AE output lies in the KWP-AD domain. */
#define TC_AES_KW_MAX_KEY_DATA_BYTES ((((uint64_t)1) << 57) - 8u)  /* 2^54 - 1 semiblocks */
#define TC_AES_KW_MAX_WRAPPED_BYTES (((uint64_t)1) << 57)          /* 2^54 semiblocks */
#define TC_AES_KWP_MAX_KEY_DATA_BYTES ((((uint64_t)1) << 32) - 8u) /* wraps to 2^29 semiblocks */
#define TC_AES_KWP_MAX_WRAPPED_BYTES (((uint64_t)1) << 32)         /* 2^29 semiblocks */

/*
 * Rules shared by the four functions below.
 *
 * kek holds a KEK length that TC_AES_KW_KEK_LENGTH_SUPPORTED accepts. Keep
 * the KEK secret (SP 800-38F section 5.1). Inputs and outputs may overlap in
 * any way, including an exact alias and the RFC 3394 layout with key data at
 * wrapped.data + 8. Each function reads its inputs in full before the first
 * output write. Keep the inputs stable for the duration of the call.
 *
 * TC_ERROR before any write, with every output unchanged, reports a NULL or
 * unsupported kek, a NULL span or buffer, a length outside the function's
 * domain, a short output capacity or a key schedule failure (in the runtime
 * S-box profile, a call before TC_AES_init_sbox). A received wrapped length
 * outside the domain is an argument error. Protocols treat TC_ERROR and
 * TC_MISMATCH from an unwrap alike as a rejection, and should limit failed
 * unwraps per KEK (SP 800-38F Appendix A.3).
 *
 * A block cipher failure after the argument checks returns TC_ERROR and
 * wipes the whole written region. In-place callers lose the input.
 */

#if TC_ENABLE_AES
/* KW-AE (SP 800-38F section 6.2 Algorithm 3, RFC 3394 section 2.2.1).
 * key_data is 16 to TC_AES_KW_MAX_KEY_DATA_BYTES bytes, a multiple of 8.
 * wrapped needs TC_AES_KW_WRAPPED_BYTES(key_data.length) bytes of capacity,
 * and exactly that many are written. Bytes past them are never written.
 * Returns TC_OK or TC_ERROR. */
TC_status TC_AES_KW_wrap(TC_bytes kek, TC_bytes key_data, TC_buffer wrapped);

/* KW-AD (SP 800-38F section 6.2 Algorithm 4, RFC 3394 section 2.2.2).
 * wrapped is 24 to TC_AES_KW_MAX_WRAPPED_BYTES bytes, a multiple of 8.
 * key_data needs TC_AES_KW_UNWRAPPED_BYTES(wrapped.length) bytes of capacity.
 * Unwrap runs W^-1 in key_data and checks the ICV afterwards. key_data holds
 * unverified bytes only during the call. Keep it private to the caller until
 * the call returns. Bytes past the working area are never written.
 * Returns TC_OK with the key data in key_data, TC_MISMATCH when the integrity
 * check fails, or TC_ERROR. Every failure after the argument checks wipes the
 * working area. */
TC_status TC_AES_KW_unwrap(TC_bytes kek, TC_bytes wrapped, TC_buffer key_data);

/* KWP-AE (SP 800-38F section 6.3 Algorithm 5, RFC 5649 section 4.1).
 * key_data is 1 to TC_AES_KWP_MAX_KEY_DATA_BYTES bytes. wrapped needs
 * TC_AES_KWP_WRAPPED_BYTES(key_data.length) bytes of capacity, and exactly
 * that many are written. Bytes past them are never written.
 * Returns TC_OK or TC_ERROR. */
TC_status TC_AES_KWP_wrap(TC_bytes kek, TC_bytes key_data, TC_buffer wrapped);

/* KWP-AD (SP 800-38F section 6.3 Algorithm 6, RFC 5649 section 4.2).
 * wrapped is 16 to TC_AES_KWP_MAX_WRAPPED_BYTES bytes, a multiple of 8.
 * key_data needs TC_AES_KW_UNWRAPPED_BYTES(wrapped.length) bytes of capacity
 * and serves as the working area. Unwrap runs W^-1 in key_data and checks the
 * ICV afterwards. key_data holds unverified bytes only during the call. Keep
 * it private to the caller until the call returns. key_data_length is
 * disjoint from the working area. It receives the key data length on TC_OK
 * only. The bytes from *key_data_length to wrapped.length - 8 then hold the
 * verified zero padding. Bytes past the working area are never written.
 * Returns TC_OK, TC_MISMATCH when the ICV, the length indicator or the
 * padding fails its check, or TC_ERROR, including for a NULL or overlapping
 * key_data_length. Every failure after the argument checks wipes the working
 * area and leaves *key_data_length unchanged. */
TC_status TC_AES_KWP_unwrap(TC_bytes kek, TC_bytes wrapped, TC_buffer key_data,
                            size_t* key_data_length);
#endif

#endif

#ifdef __cplusplus
}
#endif
#endif
