/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TINY_CRYPTO_HASH_H_
#define TINY_CRYPTO_HASH_H_

#include <tiny_crypto/common.h>

/**
 * @file hash.h
 * @brief Portable C implementation of SHA-1, SHA-224, SHA-256, SHA-384,
 *        SHA-512 and HMAC.
 *
 * Designed for microcontrollers and embedded devices. After *_final the
 * context is zeroed when TC_ZEROIZE is 1; call *_init before reuse.
 *
 * SHA-1, SHA-224 and SHA-256 live in hash.c; SHA-384 and SHA-512 share a
 * 64-bit core in sha512.c. SHA-224 pulls in the SHA-256 compression function
 * and SHA-384 the SHA-512 one; the other digest's public API stays out of the
 * build unless it is enabled itself.
 */

/*
 * Algorithm selection (define to 1/0 before including this header, or via -D).
 *
 * Default build enables SHA-256. SHA-1, SHA-224, SHA-384, SHA-512 and HMAC are
 * opt-in so unused code does not contribute to the binary. Only TC_ENABLE_*
 * names are used, so this header can coexist with vendor headers and with
 * aes.h / des.h in the same translation unit.
 */

#if (TC_ENABLE_SHA1 != 0) && (TC_ENABLE_SHA1 != 1)
  #error "TC_ENABLE_SHA1 must be 0 or 1"
#endif
#if (TC_ENABLE_SHA224 != 0) && (TC_ENABLE_SHA224 != 1)
  #error "TC_ENABLE_SHA224 must be 0 or 1"
#endif
#if (TC_ENABLE_SHA256 != 0) && (TC_ENABLE_SHA256 != 1)
  #error "TC_ENABLE_SHA256 must be 0 or 1"
#endif
#if (TC_ENABLE_SHA384 != 0) && (TC_ENABLE_SHA384 != 1)
  #error "TC_ENABLE_SHA384 must be 0 or 1"
#endif
#if (TC_ENABLE_SHA512 != 0) && (TC_ENABLE_SHA512 != 1)
  #error "TC_ENABLE_SHA512 must be 0 or 1"
#endif
#if (TC_ENABLE_HMAC != 0) && (TC_ENABLE_HMAC != 1)
  #error "TC_ENABLE_HMAC must be 0 or 1"
#endif

#if (TC_ENABLE_SHA1 == 0) && (TC_ENABLE_SHA224 == 0) && \
    (TC_ENABLE_SHA256 == 0) && (TC_ENABLE_SHA384 == 0) && (TC_ENABLE_SHA512 == 0)
  #error "at least one of TC_ENABLE_SHA1 / SHA224 / SHA256 / SHA384 / SHA512 must be 1"
#endif

#define TC_SHA1_DIGESTLEN   20  /**< SHA-1 digest length in bytes (160 bits) */
#define TC_SHA1_BLOCKLEN    64  /**< SHA-1 block length in bytes (512 bits) */
#define TC_SHA224_DIGESTLEN 28  /**< SHA-224 digest length in bytes (224 bits) */
#define TC_SHA224_BLOCKLEN  64  /**< SHA-224 block length in bytes (512 bits) */
#define TC_SHA256_DIGESTLEN 32  /**< SHA-256 digest length in bytes (256 bits) */
#define TC_SHA256_BLOCKLEN  64  /**< SHA-256 block length in bytes (512 bits) */
#define TC_SHA384_DIGESTLEN 48  /**< SHA-384 digest length in bytes (384 bits) */
#define TC_SHA384_BLOCKLEN  128 /**< SHA-384 block length in bytes (1024 bits) */
#define TC_SHA512_DIGESTLEN 64  /**< SHA-512 digest length in bytes (512 bits) */
#define TC_SHA512_BLOCKLEN  128 /**< SHA-512 block length in bytes (1024 bits) */

#if TC_ENABLE_HMAC
/*
 * Minimum accepted HMAC tag length in bytes for the one-shot HMAC_* / *_verify
 * APIs. Shorter truncations need a risk analysis (RFC 2104 section 5 asks for
 * at least half the digest length and at least 80 bits). The streaming
 * *_final API always emits the full tag and is unaffected.
 */

#if (TC_HMAC_MIN_TAG_LEN < 1)
  #error "TC_HMAC_MIN_TAG_LEN must be at least 1"
#endif
#if TC_ENABLE_SHA1 && (TC_HMAC_MIN_TAG_LEN > TC_SHA1_DIGESTLEN)
  #error "TC_HMAC_MIN_TAG_LEN must not exceed TC_SHA1_DIGESTLEN when SHA-1 is enabled"
#endif
#if TC_ENABLE_SHA224 && (TC_HMAC_MIN_TAG_LEN > TC_SHA224_DIGESTLEN)
  #error "TC_HMAC_MIN_TAG_LEN must not exceed TC_SHA224_DIGESTLEN when SHA-224 is enabled"
#endif
#if TC_ENABLE_SHA256 && (TC_HMAC_MIN_TAG_LEN > TC_SHA256_DIGESTLEN)
  #error "TC_HMAC_MIN_TAG_LEN must not exceed TC_SHA256_DIGESTLEN"
#endif
#if TC_ENABLE_SHA384 && (TC_HMAC_MIN_TAG_LEN > TC_SHA384_DIGESTLEN)
  #error "TC_HMAC_MIN_TAG_LEN must not exceed TC_SHA384_DIGESTLEN"
#endif
#if TC_ENABLE_SHA512 && (TC_HMAC_MIN_TAG_LEN > TC_SHA512_DIGESTLEN)
  #error "TC_HMAC_MIN_TAG_LEN must not exceed TC_SHA512_DIGESTLEN"
#endif
#endif /* TC_ENABLE_HMAC */

#if TC_ENABLE_SHA1
/**
 * @brief SHA-1 Context Structure
 *
 * Count is the number of message bytes absorbed so far. Buf holds the
 * partial block awaiting compression; BufLen is its fill level (< 64).
 */
struct TC_SHA1_ctx
{
  uint64_t Count;
  uint32_t State[5];
  uint8_t BufLen;
  uint8_t Buf[TC_SHA1_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA224
/**
 * @brief SHA-224 Context Structure
 *
 * Same layout as SHA-256 (SHA-224 is SHA-256 with a different IV and a
 * 28-byte output). A distinct type keeps the two APIs from being mixed.
 */
struct TC_SHA224_ctx
{
  uint64_t Count;
  uint32_t State[8];
  uint8_t BufLen;
  uint8_t Buf[TC_SHA224_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA256
/**
 * @brief SHA-256 Context Structure
 *
 * Count is the number of message bytes absorbed so far. Buf holds the
 * partial block awaiting compression; BufLen is its fill level (< 64).
 */
struct TC_SHA256_ctx
{
  uint64_t Count;
  uint32_t State[8];
  uint8_t BufLen;
  uint8_t Buf[TC_SHA256_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA384
/**
 * @brief SHA-384 Context Structure
 *
 * Same layout as SHA-512 (SHA-384 is SHA-512 with a different IV and a
 * 48-byte output). Count is the number of message bytes absorbed so far;
 * Buf holds the partial 128-byte block awaiting compression.
 */
struct TC_SHA384_ctx
{
  uint64_t Count;
  uint64_t State[8];
  uint8_t BufLen;
  uint8_t Buf[TC_SHA384_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA512
/**
 * @brief SHA-512 Context Structure
 *
 * Count is the number of message bytes absorbed so far. Buf holds the
 * partial block awaiting compression; BufLen is its fill level (< 128).
 */
struct TC_SHA512_ctx
{
  uint64_t Count;
  uint64_t State[8];
  uint8_t BufLen;
  uint8_t Buf[TC_SHA512_BLOCKLEN];
};
#endif

#if TC_ENABLE_HMAC
#if TC_ENABLE_SHA1
/**
 * @brief HMAC-SHA-1 Context Structure
 *
 * Inner absorbs (key ^ ipad) || message. OuterState is the compact hash state
 * after absorbing (key ^ opad), so the key is never retained after init.
 */
struct TC_HMAC_SHA1_ctx
{
  struct TC_SHA1_ctx Inner;
  uint32_t OuterState[5];
};
#endif

#if TC_ENABLE_SHA224
/** @brief HMAC-SHA-224 Context Structure (same shape as HMAC-SHA-256). */
struct TC_HMAC_SHA224_ctx
{
  struct TC_SHA224_ctx Inner;
  uint32_t OuterState[8];
};
#endif

#if TC_ENABLE_SHA256
/**
 * @brief HMAC-SHA-256 Context Structure
 *
 * Inner absorbs (key ^ ipad) || message. OuterState is the compact hash state
 * after absorbing (key ^ opad). The key is not retained after init.
 */
struct TC_HMAC_SHA256_ctx
{
  struct TC_SHA256_ctx Inner;
  uint32_t OuterState[8];
};
#endif

#if TC_ENABLE_SHA384
/** @brief HMAC-SHA-384 Context Structure (64-bit outer state). */
struct TC_HMAC_SHA384_ctx
{
  struct TC_SHA384_ctx Inner;
  uint64_t OuterState[8];
};
#endif

#if TC_ENABLE_SHA512
/** @brief HMAC-SHA-512 Context Structure (64-bit outer state). */
struct TC_HMAC_SHA512_ctx
{
  struct TC_SHA512_ctx Inner;
  uint64_t OuterState[8];
};
#endif
#endif /* TC_ENABLE_HMAC */

#ifdef __cplusplus
extern "C" {
#endif

#if TC_ENABLE_SHA1
/* --- SHA-1 API --- */

/**
 * @brief Initialize a SHA-1 context.
 * @param ctx Pointer to SHA-1 context structure.
 */
TC_status TC_SHA1_init(struct TC_SHA1_ctx* ctx);

/**
 * @brief Absorb message bytes into a SHA-1 context.
 * @param ctx Pointer to initialized SHA-1 context.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Number of bytes to absorb.
 * @return TC_OK, TC_ERROR on a TC_STRICT NULL failure, or TC_ERROR if
 *         absorbing len would overflow the 64-bit bit counter (2^61 − 1 bytes).
 */
TC_status TC_SHA1_update(struct TC_SHA1_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize a SHA-1 computation and emit the digest.
 *
 * When TC_ZEROIZE is 1 the context is wiped on return and must be
 * re-initialized before reuse.
 * @param ctx Pointer to initialized SHA-1 context.
 * @param digest Output buffer of TC_SHA1_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_SHA1_final(struct TC_SHA1_ctx* ctx, uint8_t* digest);

/* Wipe a SHA-1 context (state, byte count and pending block). */
void TC_SHA1_ctx_clear(struct TC_SHA1_ctx* ctx);

/**
 * @brief One-shot SHA-1 of a message.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Message length in bytes.
 * @param digest Output buffer of TC_SHA1_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_SHA1_digest(const uint8_t* data, size_t len, uint8_t* digest);
#endif /* TC_ENABLE_SHA1 */

#if TC_ENABLE_SHA224
/* --- SHA-224 API --- */

/**
 * @brief Initialize a SHA-224 context.
 * @param ctx Pointer to SHA-224 context structure.
 */
TC_status TC_SHA224_init(struct TC_SHA224_ctx* ctx);

/**
 * @brief Absorb message bytes into a SHA-224 context.
 * @param ctx Pointer to initialized SHA-224 context.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Number of bytes to absorb.
 * @return TC_OK, TC_ERROR on a TC_STRICT NULL failure, or TC_ERROR if
 *         absorbing len would overflow the 64-bit bit counter (2^61 − 1 bytes).
 */
TC_status TC_SHA224_update(struct TC_SHA224_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize a SHA-224 computation and emit the digest.
 *
 * When TC_ZEROIZE is 1 the context is wiped on return and must be
 * re-initialized before reuse.
 * @param ctx Pointer to initialized SHA-224 context.
 * @param digest Output buffer of TC_SHA224_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_SHA224_final(struct TC_SHA224_ctx* ctx, uint8_t* digest);

/* Wipe a SHA-224 context (state, byte count and pending block). */
void TC_SHA224_ctx_clear(struct TC_SHA224_ctx* ctx);

/**
 * @brief One-shot SHA-224 of a message.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Message length in bytes.
 * @param digest Output buffer of TC_SHA224_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_SHA224_digest(const uint8_t* data, size_t len, uint8_t* digest);
#endif /* TC_ENABLE_SHA224 */

#if TC_ENABLE_SHA256
/* --- SHA-256 API --- */

/**
 * @brief Initialize a SHA-256 context.
 * @param ctx Pointer to SHA-256 context structure.
 */
TC_status TC_SHA256_init(struct TC_SHA256_ctx* ctx);

/**
 * @brief Absorb message bytes into a SHA-256 context.
 * @param ctx Pointer to initialized SHA-256 context.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Number of bytes to absorb.
 * @return TC_OK, TC_ERROR on a TC_STRICT NULL failure, or TC_ERROR if
 *         absorbing len would overflow the 64-bit bit counter (2^61 − 1 bytes).
 */
TC_status TC_SHA256_update(struct TC_SHA256_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize a SHA-256 computation and emit the digest.
 *
 * When TC_ZEROIZE is 1 the context is wiped on return and must be
 * re-initialized before reuse.
 * @param ctx Pointer to initialized SHA-256 context.
 * @param digest Output buffer of TC_SHA256_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_SHA256_final(struct TC_SHA256_ctx* ctx, uint8_t* digest);

/* Wipe a SHA-256 context (state, byte count and pending block). */
void TC_SHA256_ctx_clear(struct TC_SHA256_ctx* ctx);

/**
 * @brief One-shot SHA-256 of a message.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Message length in bytes.
 * @param digest Output buffer of TC_SHA256_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_SHA256_digest(const uint8_t* data, size_t len, uint8_t* digest);
#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA384
/* --- SHA-384 API --- */

/**
 * @brief Initialize a SHA-384 context.
 * @param ctx Pointer to SHA-384 context structure.
 */
TC_status TC_SHA384_init(struct TC_SHA384_ctx* ctx);

/**
 * @brief Absorb message bytes into a SHA-384 context.
 * @param ctx Pointer to initialized SHA-384 context.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Number of bytes to absorb.
 * @return TC_OK, TC_ERROR on a TC_STRICT NULL failure, or TC_ERROR if
 *         absorbing len would overflow the 128-bit bit counter (2^64 − 1 bytes).
 */
TC_status TC_SHA384_update(struct TC_SHA384_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize a SHA-384 computation and emit the digest.
 *
 * When TC_ZEROIZE is 1 the context is wiped on return and must be
 * re-initialized before reuse.
 * @param ctx Pointer to initialized SHA-384 context.
 * @param digest Output buffer of TC_SHA384_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_SHA384_final(struct TC_SHA384_ctx* ctx, uint8_t* digest);

/* Wipe a SHA-384 context (state, byte count and pending block). */
void TC_SHA384_ctx_clear(struct TC_SHA384_ctx* ctx);

/**
 * @brief One-shot SHA-384 of a message.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Message length in bytes.
 * @param digest Output buffer of TC_SHA384_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_SHA384_digest(const uint8_t* data, size_t len, uint8_t* digest);
#endif /* TC_ENABLE_SHA384 */

#if TC_ENABLE_SHA512
/* --- SHA-512 API --- */

/**
 * @brief Initialize a SHA-512 context.
 * @param ctx Pointer to SHA-512 context structure.
 */
TC_status TC_SHA512_init(struct TC_SHA512_ctx* ctx);

/**
 * @brief Absorb message bytes into a SHA-512 context.
 * @param ctx Pointer to initialized SHA-512 context.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Number of bytes to absorb.
 * @return TC_OK, TC_ERROR on a TC_STRICT NULL failure, or TC_ERROR if
 *         absorbing len would overflow the 128-bit bit counter (2^64 − 1 bytes).
 */
TC_status TC_SHA512_update(struct TC_SHA512_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize a SHA-512 computation and emit the digest.
 *
 * When TC_ZEROIZE is 1 the context is wiped on return and must be
 * re-initialized before reuse.
 * @param ctx Pointer to initialized SHA-512 context.
 * @param digest Output buffer of TC_SHA512_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_SHA512_final(struct TC_SHA512_ctx* ctx, uint8_t* digest);

/* Wipe a SHA-512 context (state, byte count and pending block). */
void TC_SHA512_ctx_clear(struct TC_SHA512_ctx* ctx);

/**
 * @brief One-shot SHA-512 of a message.
 * @param data Pointer to message bytes (may be NULL when len is 0).
 * @param len Message length in bytes.
 * @param digest Output buffer of TC_SHA512_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_SHA512_digest(const uint8_t* data, size_t len, uint8_t* digest);
#endif /* TC_ENABLE_SHA512 */

#if TC_ENABLE_HMAC
/* --- HMAC API (RFC 2104 / FIPS 198-1) --- */

#if TC_ENABLE_SHA1
/**
 * @brief Initialize an HMAC-SHA-1 context with a key.
 *
 * Keys longer than TC_SHA1_BLOCKLEN are hashed first; shorter keys are
 * zero-padded. A zero-length key is accepted (key may then be NULL).
 * @param ctx Pointer to HMAC-SHA-1 context structure.
 * @param key Pointer to key bytes.
 * @param keylen Key length in bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA1_init(struct TC_HMAC_SHA1_ctx* ctx, const uint8_t* key, size_t keylen);

/**
 * @brief Absorb message bytes into an HMAC-SHA-1 context.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_HMAC_SHA1_update(struct TC_HMAC_SHA1_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize an HMAC-SHA-1 computation and emit the full tag.
 *
 * One-shot: the context is consumed. Call TC_HMAC_SHA1_init with the key
 * before reuse. When TC_ZEROIZE is 1 both inner contexts are wiped.
 * @param ctx Pointer to initialized HMAC-SHA-1 context.
 * @param tag Output buffer of TC_SHA1_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_HMAC_SHA1_final(struct TC_HMAC_SHA1_ctx* ctx, uint8_t* tag);

/* Wipe an HMAC-SHA-1 context. */
void TC_HMAC_SHA1_ctx_clear(struct TC_HMAC_SHA1_ctx* ctx);

/**
 * @brief One-shot HMAC-SHA-1 with optional truncation.
 * @param key Pointer to key bytes (may be NULL when keylen is 0).
 * @param keylen Key length in bytes.
 * @param msg Pointer to message bytes (may be NULL when msg_len is 0).
 * @param msg_len Message length in bytes.
 * @param tag Output buffer of tag_len bytes.
 * @param tag_len Tag length; TC_HMAC_MIN_TAG_LEN..TC_SHA1_DIGESTLEN.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA1_digest(const uint8_t* key, size_t keylen,
                     const uint8_t* msg, size_t msg_len,
                     uint8_t* tag, size_t tag_len);

/**
 * @brief Verify an HMAC-SHA-1 tag via TC_ct_equal (constant-time in the tag bytes).
 * @return TC_OK on match, TC_MISMATCH on a well-formed miss, TC_ERROR
 *         on invalid arguments (NULL tag, tag_len out of range, …).
 */
TC_status TC_HMAC_SHA1_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                     const uint8_t* tag, size_t tag_len);
#endif /* TC_ENABLE_SHA1 */

#if TC_ENABLE_SHA224
/**
 * @brief Initialize an HMAC-SHA-224 context with a key.
 *
 * Keys longer than TC_SHA224_BLOCKLEN are hashed first; shorter keys are
 * zero-padded. A zero-length key is accepted (key may then be NULL).
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA224_init(struct TC_HMAC_SHA224_ctx* ctx, const uint8_t* key, size_t keylen);

/** @brief Absorb message bytes; TC_ERROR only on a TC_STRICT NULL failure. */
TC_status TC_HMAC_SHA224_update(struct TC_HMAC_SHA224_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize and emit the full TC_SHA224_DIGESTLEN-byte tag.
 *
 * One-shot: the context is consumed. Call TC_HMAC_SHA224_init with the key
 * before reuse. When TC_ZEROIZE is 1 both inner contexts are wiped.
 */
TC_status TC_HMAC_SHA224_final(struct TC_HMAC_SHA224_ctx* ctx, uint8_t* tag);

/* Wipe an HMAC-SHA-224 context. */
void TC_HMAC_SHA224_ctx_clear(struct TC_HMAC_SHA224_ctx* ctx);

/**
 * @brief One-shot HMAC-SHA-224 with optional truncation.
 * @param tag_len Tag length; TC_HMAC_MIN_TAG_LEN..TC_SHA224_DIGESTLEN.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA224_digest(const uint8_t* key, size_t keylen,
                       const uint8_t* msg, size_t msg_len,
                       uint8_t* tag, size_t tag_len);

/**
 * @brief Verify an HMAC-SHA-224 tag via TC_ct_equal (constant-time in the tag bytes).
 * @return TC_OK on match, TC_MISMATCH on a well-formed miss, TC_ERROR
 *         on invalid arguments (NULL tag, tag_len out of range, …).
 */
TC_status TC_HMAC_SHA224_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len);
#endif /* TC_ENABLE_SHA224 */

#if TC_ENABLE_SHA256
/**
 * @brief Initialize an HMAC-SHA-256 context with a key.
 *
 * Keys longer than TC_SHA256_BLOCKLEN are hashed first; shorter keys are
 * zero-padded. A zero-length key is accepted (key may then be NULL).
 * @param ctx Pointer to HMAC-SHA-256 context structure.
 * @param key Pointer to key bytes.
 * @param keylen Key length in bytes.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA256_init(struct TC_HMAC_SHA256_ctx* ctx, const uint8_t* key, size_t keylen);

/**
 * @brief Absorb message bytes into an HMAC-SHA-256 context.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_HMAC_SHA256_update(struct TC_HMAC_SHA256_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize an HMAC-SHA-256 computation and emit the full tag.
 *
 * One-shot: the context is consumed. Call TC_HMAC_SHA256_init with the key
 * before reuse. When TC_ZEROIZE is 1 both inner contexts are wiped.
 * @param ctx Pointer to initialized HMAC-SHA-256 context.
 * @param tag Output buffer of TC_SHA256_DIGESTLEN bytes.
 * @return TC_OK, or TC_ERROR on a TC_STRICT NULL failure.
 */
TC_status TC_HMAC_SHA256_final(struct TC_HMAC_SHA256_ctx* ctx, uint8_t* tag);

/* Wipe an HMAC-SHA-256 context. */
void TC_HMAC_SHA256_ctx_clear(struct TC_HMAC_SHA256_ctx* ctx);

/**
 * @brief One-shot HMAC-SHA-256 with optional truncation.
 * @param key Pointer to key bytes (may be NULL when keylen is 0).
 * @param keylen Key length in bytes.
 * @param msg Pointer to message bytes (may be NULL when msg_len is 0).
 * @param msg_len Message length in bytes.
 * @param tag Output buffer of tag_len bytes.
 * @param tag_len Tag length; TC_HMAC_MIN_TAG_LEN..TC_SHA256_DIGESTLEN.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA256_digest(const uint8_t* key, size_t keylen,
                       const uint8_t* msg, size_t msg_len,
                       uint8_t* tag, size_t tag_len);

/**
 * @brief Verify an HMAC-SHA-256 tag via TC_ct_equal (constant-time in the tag bytes).
 * @return TC_OK on match, TC_MISMATCH on a well-formed miss, TC_ERROR
 *         on invalid arguments (NULL tag, tag_len out of range, …).
 */
TC_status TC_HMAC_SHA256_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len);
#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA384
/**
 * @brief Initialize an HMAC-SHA-384 context with a key.
 *
 * Keys longer than TC_SHA384_BLOCKLEN are hashed first; shorter keys are
 * zero-padded. A zero-length key is accepted (key may then be NULL).
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA384_init(struct TC_HMAC_SHA384_ctx* ctx, const uint8_t* key, size_t keylen);

/** @brief Absorb message bytes; TC_ERROR only on a TC_STRICT NULL failure. */
TC_status TC_HMAC_SHA384_update(struct TC_HMAC_SHA384_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize and emit the full TC_SHA384_DIGESTLEN-byte tag.
 *
 * One-shot: the context is consumed. Call TC_HMAC_SHA384_init with the key
 * before reuse. When TC_ZEROIZE is 1 both inner contexts are wiped.
 */
TC_status TC_HMAC_SHA384_final(struct TC_HMAC_SHA384_ctx* ctx, uint8_t* tag);

/* Wipe an HMAC-SHA-384 context. */
void TC_HMAC_SHA384_ctx_clear(struct TC_HMAC_SHA384_ctx* ctx);

/**
 * @brief One-shot HMAC-SHA-384 with optional truncation.
 * @param tag_len Tag length; TC_HMAC_MIN_TAG_LEN..TC_SHA384_DIGESTLEN.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA384_digest(const uint8_t* key, size_t keylen,
                       const uint8_t* msg, size_t msg_len,
                       uint8_t* tag, size_t tag_len);

/**
 * @brief Verify an HMAC-SHA-384 tag via TC_ct_equal (constant-time in the tag bytes).
 * @return TC_OK on match, TC_MISMATCH on a well-formed miss, TC_ERROR
 *         on invalid arguments (NULL tag, tag_len out of range, …).
 */
TC_status TC_HMAC_SHA384_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len);
#endif /* TC_ENABLE_SHA384 */

#if TC_ENABLE_SHA512
/**
 * @brief Initialize an HMAC-SHA-512 context with a key.
 *
 * Keys longer than TC_SHA512_BLOCKLEN are hashed first; shorter keys are
 * zero-padded. A zero-length key is accepted (key may then be NULL).
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA512_init(struct TC_HMAC_SHA512_ctx* ctx, const uint8_t* key, size_t keylen);

/** @brief Absorb message bytes; TC_ERROR only on a TC_STRICT NULL failure. */
TC_status TC_HMAC_SHA512_update(struct TC_HMAC_SHA512_ctx* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize and emit the full TC_SHA512_DIGESTLEN-byte tag.
 *
 * One-shot: the context is consumed. Call TC_HMAC_SHA512_init with the key
 * before reuse. When TC_ZEROIZE is 1 both inner contexts are wiped.
 */
TC_status TC_HMAC_SHA512_final(struct TC_HMAC_SHA512_ctx* ctx, uint8_t* tag);

/* Wipe an HMAC-SHA-512 context. */
void TC_HMAC_SHA512_ctx_clear(struct TC_HMAC_SHA512_ctx* ctx);

/**
 * @brief One-shot HMAC-SHA-512 with optional truncation.
 * @param tag_len Tag length; TC_HMAC_MIN_TAG_LEN..TC_SHA512_DIGESTLEN.
 * @return TC_OK, or TC_ERROR on invalid arguments.
 */
TC_status TC_HMAC_SHA512_digest(const uint8_t* key, size_t keylen,
                       const uint8_t* msg, size_t msg_len,
                       uint8_t* tag, size_t tag_len);

/**
 * @brief Verify an HMAC-SHA-512 tag via TC_ct_equal (constant-time in the tag bytes).
 * @return TC_OK on match, TC_MISMATCH on a well-formed miss, TC_ERROR
 *         on invalid arguments (NULL tag, tag_len out of range, …).
 */
TC_status TC_HMAC_SHA512_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len);
#endif /* TC_ENABLE_SHA512 */

#endif /* TC_ENABLE_HMAC */

#ifdef __cplusplus
}
#endif

#endif /* TINY_CRYPTO_HASH_H_ */
