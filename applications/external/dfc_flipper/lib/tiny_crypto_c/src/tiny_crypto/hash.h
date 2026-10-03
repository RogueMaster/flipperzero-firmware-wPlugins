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
 *        SHA-512 and HMAC (FIPS 180-4, FIPS 198-1).
 *
 * The profile in config.h selects the digests with TC_ENABLE_SHA1 through
 * TC_ENABLE_SHA512 and HMAC with TC_ENABLE_HMAC. This header declares only
 * the enabled algorithms and is empty when no SHA is enabled.
 * Standards: FIPS 180-4 sections 5.1 (padding), 6.1 (SHA-1), 6.2 (SHA-256),
 * 6.3 (SHA-224), 6.4 (SHA-512) and 6.5 (SHA-384). FIPS 198-1 section 4
 * (HMAC). SP 800-107 Rev. 1 section 5.3 (truncated HMAC tags).
 * Work: every function charges no work budget.
 * Library-wide contracts: docs/api.md. C++ classes: hash.hpp and docs/cpp.md.
 *
 * Inputs are borrowed TC_bytes spans. A span may have NULL data only when its
 * length is 0. Fixed-length digests and full HMAC tags are written to arrays of
 * the digest length. A truncated one-shot HMAC tag is a TC_buffer whose
 * capacity is the tag length.
 *
 * Status contract for every SHA and HMAC function below:
 *
 *   init     TC_OK, or TC_ERROR for a NULL context. HMAC init also rejects a
 *            key span with NULL data and a nonzero length, and a key that
 *            overlaps the context. A failed HMAC init with a non-NULL context
 *            leaves the context wiped and inactive.
 *   update   TC_OK, or TC_ERROR for a NULL context, a data span with NULL
 *            data and a nonzero length, data that overlaps the context, an
 *            inactive context (never initialized, finalized or cleared), a
 *            corrupted context, or input past the message limit (2^61 - 1
 *            bytes for SHA-1/224/256 and 2^64 - 1 bytes for SHA-384/512). A
 *            rejected update leaves the context unchanged.
 *   final    TC_OK, or TC_ERROR for a NULL context, a NULL output, an output
 *            that overlaps the context, or an inactive or corrupted context. A
 *            rejected final leaves the context unchanged. A successful final
 *            writes the full digest or tag and wipes the context, so call init
 *            before reuse.
 *   digest   One-shot hash or HMAC. TC_OK, or TC_ERROR for a NULL output,
 *            an invalid input span, a message past the limit, an HMAC key
 *            error, or an HMAC tag.capacity outside the documented default
 *            or explicit short-tag range. A truncated HMAC tag keeps the
 *            leftmost bytes. The output is written last, so it may
 *            overlap the message or key. A failed call leaves the output
 *            unchanged.
 *   verify   TC_OK on a match, TC_MISMATCH on a well-formed mismatch, and
 *            TC_ERROR for the digest errors or a tag span with NULL data. The
 *            tag length follows the digest tag.capacity rule. The compare runs
 *            in constant time over tag.length bytes.
 *   clear    Wipes the context and leaves it inactive. NULL is ignored.
 *
 * A context must be disjoint from every buffer passed with it. One-shot calls
 * keep their context on the stack and wipe it before return. An HMAC context
 * holds the inner hash context and the outer hash state after the key block,
 * and never a copy of the key.
 *
 * SHA-1, SHA-224 and SHA-256 live in hash.c. SHA-384 and SHA-512 share a
 * 64-bit core in sha512.c. SHA-224 pulls in the SHA-256 compression function
 * and SHA-384 the SHA-512 one.
 */

#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || TC_ENABLE_SHA384 || TC_ENABLE_SHA512

#define TC_SHA1_DIGESTLEN 20   /**< SHA-1 digest length in bytes (160 bits) */
#define TC_SHA1_BLOCKLEN 64    /**< SHA-1 block length in bytes (512 bits) */
#define TC_SHA224_DIGESTLEN 28 /**< SHA-224 digest length in bytes (224 bits) */
#define TC_SHA224_BLOCKLEN 64  /**< SHA-224 block length in bytes (512 bits) */
#define TC_SHA256_DIGESTLEN 32 /**< SHA-256 digest length in bytes (256 bits) */
#define TC_SHA256_BLOCKLEN 64  /**< SHA-256 block length in bytes (512 bits) */
#define TC_SHA384_DIGESTLEN 48 /**< SHA-384 digest length in bytes (384 bits) */
#define TC_SHA384_BLOCKLEN 128 /**< SHA-384 block length in bytes (1024 bits) */
#define TC_SHA512_DIGESTLEN 64 /**< SHA-512 digest length in bytes (512 bits) */
#define TC_SHA512_BLOCKLEN 128 /**< SHA-512 block length in bytes (1024 bits) */

#if TC_ENABLE_SHA1
/**
 * @brief SHA-1 Context Structure
 *
 * count is the number of message bytes absorbed so far. buf holds the
 * partial block awaiting compression, and buf_len is its fill level (< 64).
 */
struct TC_SHA1_ctx {
  uint64_t count;
  uint32_t state[5];
  uint8_t buf_len;
  uint8_t active;
  uint8_t buf[TC_SHA1_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA224
/**
 * @brief SHA-224 Context Structure
 *
 * Same layout as SHA-256 (SHA-224 is SHA-256 with a different IV and a
 * 28-byte output). A distinct type keeps the two APIs from being mixed.
 */
struct TC_SHA224_ctx {
  uint64_t count;
  uint32_t state[8];
  uint8_t buf_len;
  uint8_t active;
  uint8_t buf[TC_SHA224_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA256
/**
 * @brief SHA-256 Context Structure
 *
 * count is the number of message bytes absorbed so far. buf holds the
 * partial block awaiting compression, and buf_len is its fill level (< 64).
 */
struct TC_SHA256_ctx {
  uint64_t count;
  uint32_t state[8];
  uint8_t buf_len;
  uint8_t active;
  uint8_t buf[TC_SHA256_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA384
/**
 * @brief SHA-384 Context Structure
 *
 * Same layout as SHA-512 (SHA-384 is SHA-512 with a different IV and a
 * 48-byte output). count is the number of message bytes absorbed so far.
 * buf holds the partial 128-byte block awaiting compression.
 */
struct TC_SHA384_ctx {
  uint64_t count;
  uint64_t state[8];
  uint8_t buf_len;
  uint8_t active;
  uint8_t buf[TC_SHA384_BLOCKLEN];
};
#endif

#if TC_ENABLE_SHA512
/**
 * @brief SHA-512 Context Structure
 *
 * count is the number of message bytes absorbed so far. buf holds the
 * partial block awaiting compression, and buf_len is its fill level (< 128).
 */
struct TC_SHA512_ctx {
  uint64_t count;
  uint64_t state[8];
  uint8_t buf_len;
  uint8_t active;
  uint8_t buf[TC_SHA512_BLOCKLEN];
};
#endif

#if TC_ENABLE_HMAC
#if TC_ENABLE_SHA1
/**
 * @brief HMAC-SHA-1 Context Structure
 *
 * inner absorbs (key ^ ipad) || message. outer_state is the compact hash state
 * after absorbing (key ^ opad), so the context holds no copy of the key after init.
 */
struct TC_HMAC_SHA1_ctx {
  struct TC_SHA1_ctx inner;
  uint32_t outer_state[5];
};
#endif

#if TC_ENABLE_SHA224
/** @brief HMAC-SHA-224 Context Structure (same shape as HMAC-SHA-256). */
struct TC_HMAC_SHA224_ctx {
  struct TC_SHA224_ctx inner;
  uint32_t outer_state[8];
};
#endif

#if TC_ENABLE_SHA256
/**
 * @brief HMAC-SHA-256 Context Structure
 *
 * inner absorbs (key ^ ipad) || message. outer_state is the compact hash state
 * after absorbing (key ^ opad). The context holds no copy of the key after init.
 */
struct TC_HMAC_SHA256_ctx {
  struct TC_SHA256_ctx inner;
  uint32_t outer_state[8];
};
#endif

#if TC_ENABLE_SHA384
/** @brief HMAC-SHA-384 Context Structure (64-bit outer state). */
struct TC_HMAC_SHA384_ctx {
  struct TC_SHA384_ctx inner;
  uint64_t outer_state[8];
};
#endif

#if TC_ENABLE_SHA512
/** @brief HMAC-SHA-512 Context Structure (64-bit outer state). */
struct TC_HMAC_SHA512_ctx {
  struct TC_SHA512_ctx inner;
  uint64_t outer_state[8];
};
#endif
#endif /* TC_ENABLE_HMAC */

/* Storage for any enabled SHA-1 or SHA-2 context, for code that selects the
 * hash at run time. Its size is the largest enabled context. A pointer to the
 * union converts to a pointer to each member. */
typedef union {
  uint8_t unused;
#if TC_ENABLE_SHA1
  struct TC_SHA1_ctx sha1;
#endif
#if TC_ENABLE_SHA224
  struct TC_SHA224_ctx sha224;
#endif
#if TC_ENABLE_SHA256
  struct TC_SHA256_ctx sha256;
#endif
#if TC_ENABLE_SHA384
  struct TC_SHA384_ctx sha384;
#endif
#if TC_ENABLE_SHA512
  struct TC_SHA512_ctx sha512;
#endif
} TC_hash_context;

#if TC_ENABLE_HMAC
/* Storage for any enabled HMAC context, sized like TC_hash_context. */
typedef union {
  uint8_t unused;
#if TC_ENABLE_SHA1
  struct TC_HMAC_SHA1_ctx sha1;
#endif
#if TC_ENABLE_SHA224
  struct TC_HMAC_SHA224_ctx sha224;
#endif
#if TC_ENABLE_SHA256
  struct TC_HMAC_SHA256_ctx sha256;
#endif
#if TC_ENABLE_SHA384
  struct TC_HMAC_SHA384_ctx sha384;
#endif
#if TC_ENABLE_SHA512
  struct TC_HMAC_SHA512_ctx sha512;
#endif
} TC_HMAC_context;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if TC_ENABLE_SHA1
/* --- SHA-1 --- */

/* Start a SHA-1 message (FIPS 180-4 section 6.1). Also restarts a live
 * context. */
TC_status TC_SHA1_init(struct TC_SHA1_ctx* ctx);

/* Absorb data. data.data may be NULL only when data.length is 0. The total
 * message is limited to 2^61 - 1 bytes. */
TC_status TC_SHA1_update(struct TC_SHA1_ctx* ctx, TC_bytes data);

/* Write the TC_SHA1_DIGESTLEN-byte digest and wipe the context. */
TC_status TC_SHA1_final(struct TC_SHA1_ctx* ctx, uint8_t digest[TC_SHA1_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_SHA1_ctx_clear(struct TC_SHA1_ctx* ctx);

/* One-shot SHA-1. digest may overlap data. */
TC_status TC_SHA1_digest(TC_bytes data, uint8_t digest[TC_SHA1_DIGESTLEN]);
#endif /* TC_ENABLE_SHA1 */

#if TC_ENABLE_SHA224
/* --- SHA-224 --- */

/* Start a SHA-224 message (FIPS 180-4 section 6.3). Also restarts a live
 * context. */
TC_status TC_SHA224_init(struct TC_SHA224_ctx* ctx);

/* Absorb data. data.data may be NULL only when data.length is 0. The total
 * message is limited to 2^61 - 1 bytes. */
TC_status TC_SHA224_update(struct TC_SHA224_ctx* ctx, TC_bytes data);

/* Write the TC_SHA224_DIGESTLEN-byte digest and wipe the context. */
TC_status TC_SHA224_final(struct TC_SHA224_ctx* ctx, uint8_t digest[TC_SHA224_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_SHA224_ctx_clear(struct TC_SHA224_ctx* ctx);

/* One-shot SHA-224. digest may overlap data. */
TC_status TC_SHA224_digest(TC_bytes data, uint8_t digest[TC_SHA224_DIGESTLEN]);
#endif /* TC_ENABLE_SHA224 */

#if TC_ENABLE_SHA256
/* --- SHA-256 --- */

/* Start a SHA-256 message (FIPS 180-4 section 6.2). Also restarts a live
 * context. */
TC_status TC_SHA256_init(struct TC_SHA256_ctx* ctx);

/* Absorb data. data.data may be NULL only when data.length is 0. The total
 * message is limited to 2^61 - 1 bytes. */
TC_status TC_SHA256_update(struct TC_SHA256_ctx* ctx, TC_bytes data);

/* Write the TC_SHA256_DIGESTLEN-byte digest and wipe the context. */
TC_status TC_SHA256_final(struct TC_SHA256_ctx* ctx, uint8_t digest[TC_SHA256_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_SHA256_ctx_clear(struct TC_SHA256_ctx* ctx);

/* One-shot SHA-256. digest may overlap data. */
TC_status TC_SHA256_digest(TC_bytes data, uint8_t digest[TC_SHA256_DIGESTLEN]);
#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA384
/* --- SHA-384 --- */

/* Start a SHA-384 message (FIPS 180-4 section 6.5). Also restarts a live
 * context. */
TC_status TC_SHA384_init(struct TC_SHA384_ctx* ctx);

/* Absorb data. data.data may be NULL only when data.length is 0. The total
 * message is limited to 2^64 - 1 bytes. */
TC_status TC_SHA384_update(struct TC_SHA384_ctx* ctx, TC_bytes data);

/* Write the TC_SHA384_DIGESTLEN-byte digest and wipe the context. */
TC_status TC_SHA384_final(struct TC_SHA384_ctx* ctx, uint8_t digest[TC_SHA384_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_SHA384_ctx_clear(struct TC_SHA384_ctx* ctx);

/* One-shot SHA-384. digest may overlap data. */
TC_status TC_SHA384_digest(TC_bytes data, uint8_t digest[TC_SHA384_DIGESTLEN]);
#endif /* TC_ENABLE_SHA384 */

#if TC_ENABLE_SHA512
/* --- SHA-512 --- */

/* Start a SHA-512 message (FIPS 180-4 section 6.4). Also restarts a live
 * context. */
TC_status TC_SHA512_init(struct TC_SHA512_ctx* ctx);

/* Absorb data. data.data may be NULL only when data.length is 0. The total
 * message is limited to 2^64 - 1 bytes. */
TC_status TC_SHA512_update(struct TC_SHA512_ctx* ctx, TC_bytes data);

/* Write the TC_SHA512_DIGESTLEN-byte digest and wipe the context. */
TC_status TC_SHA512_final(struct TC_SHA512_ctx* ctx, uint8_t digest[TC_SHA512_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_SHA512_ctx_clear(struct TC_SHA512_ctx* ctx);

/* One-shot SHA-512. digest may overlap data. */
TC_status TC_SHA512_digest(TC_bytes data, uint8_t digest[TC_SHA512_DIGESTLEN]);
#endif /* TC_ENABLE_SHA512 */

#if TC_ENABLE_HMAC
/* --- HMAC (FIPS 198-1) --- */

#if TC_ENABLE_SHA1
/* Key an HMAC-SHA-1 context. Keys longer than TC_SHA1_BLOCKLEN bytes are
 * hashed first and shorter keys are zero-padded (FIPS 198-1 section 4). An
 * empty key is accepted. */
TC_status TC_HMAC_SHA1_init(struct TC_HMAC_SHA1_ctx* ctx, TC_bytes key);

/* Absorb message bytes under the update contract. */
TC_status TC_HMAC_SHA1_update(struct TC_HMAC_SHA1_ctx* ctx, TC_bytes data);

/* Write the full TC_SHA1_DIGESTLEN-byte tag and wipe the context. Call init
 * with the key before reuse. */
TC_status TC_HMAC_SHA1_final(struct TC_HMAC_SHA1_ctx* ctx, uint8_t tag[TC_SHA1_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_HMAC_SHA1_ctx_clear(struct TC_HMAC_SHA1_ctx* ctx);

/* One-shot HMAC-SHA-1 truncated to tag.capacity bytes. The default call takes
 * max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)..TC_SHA1_DIGESTLEN; the short-tag
 * call takes 1..that minimum - 1. */
TC_status TC_HMAC_SHA1_digest(TC_bytes key, TC_bytes message, TC_buffer tag);
TC_status TC_HMAC_SHA1_digest_short_tag(TC_bytes key, TC_bytes message, TC_buffer tag);

/* Recompute the tag, truncated to tag.length bytes, and compare in
 * constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR under the verify
 * contract. */
TC_status TC_HMAC_SHA1_verify(TC_bytes key, TC_bytes message, TC_bytes tag);
TC_status TC_HMAC_SHA1_verify_short_tag(TC_bytes key, TC_bytes message, TC_bytes tag);
#endif /* TC_ENABLE_SHA1 */

#if TC_ENABLE_SHA224
/* Key an HMAC-SHA-224 context. Keys longer than TC_SHA224_BLOCKLEN bytes are
 * hashed first and shorter keys are zero-padded (FIPS 198-1 section 4). An
 * empty key is accepted. */
TC_status TC_HMAC_SHA224_init(struct TC_HMAC_SHA224_ctx* ctx, TC_bytes key);

/* Absorb message bytes under the update contract. */
TC_status TC_HMAC_SHA224_update(struct TC_HMAC_SHA224_ctx* ctx, TC_bytes data);

/* Write the full TC_SHA224_DIGESTLEN-byte tag and wipe the context. Call init
 * with the key before reuse. */
TC_status TC_HMAC_SHA224_final(struct TC_HMAC_SHA224_ctx* ctx, uint8_t tag[TC_SHA224_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_HMAC_SHA224_ctx_clear(struct TC_HMAC_SHA224_ctx* ctx);

/* One-shot HMAC-SHA-224 truncated to tag.capacity bytes. The default call takes
 * max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)..TC_SHA224_DIGESTLEN; the short-tag
 * call takes 1..that minimum - 1. */
TC_status TC_HMAC_SHA224_digest(TC_bytes key, TC_bytes message, TC_buffer tag);
TC_status TC_HMAC_SHA224_digest_short_tag(TC_bytes key, TC_bytes message, TC_buffer tag);

/* Recompute the tag, truncated to tag.length bytes, and compare in
 * constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR under the verify
 * contract. */
TC_status TC_HMAC_SHA224_verify(TC_bytes key, TC_bytes message, TC_bytes tag);
TC_status TC_HMAC_SHA224_verify_short_tag(TC_bytes key, TC_bytes message, TC_bytes tag);
#endif /* TC_ENABLE_SHA224 */

#if TC_ENABLE_SHA256
/* Key an HMAC-SHA-256 context. Keys longer than TC_SHA256_BLOCKLEN bytes are
 * hashed first and shorter keys are zero-padded (FIPS 198-1 section 4). An
 * empty key is accepted. */
TC_status TC_HMAC_SHA256_init(struct TC_HMAC_SHA256_ctx* ctx, TC_bytes key);

/* Absorb message bytes under the update contract. */
TC_status TC_HMAC_SHA256_update(struct TC_HMAC_SHA256_ctx* ctx, TC_bytes data);

/* Write the full TC_SHA256_DIGESTLEN-byte tag and wipe the context. Call init
 * with the key before reuse. */
TC_status TC_HMAC_SHA256_final(struct TC_HMAC_SHA256_ctx* ctx, uint8_t tag[TC_SHA256_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_HMAC_SHA256_ctx_clear(struct TC_HMAC_SHA256_ctx* ctx);

/* One-shot HMAC-SHA-256 truncated to tag.capacity bytes. The default call takes
 * max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)..TC_SHA256_DIGESTLEN; the short-tag
 * call takes 1..that minimum - 1. */
TC_status TC_HMAC_SHA256_digest(TC_bytes key, TC_bytes message, TC_buffer tag);
TC_status TC_HMAC_SHA256_digest_short_tag(TC_bytes key, TC_bytes message, TC_buffer tag);

/* Recompute the tag, truncated to tag.length bytes, and compare in
 * constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR under the verify
 * contract. */
TC_status TC_HMAC_SHA256_verify(TC_bytes key, TC_bytes message, TC_bytes tag);
TC_status TC_HMAC_SHA256_verify_short_tag(TC_bytes key, TC_bytes message, TC_bytes tag);
#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA384
/* Key an HMAC-SHA-384 context. Keys longer than TC_SHA384_BLOCKLEN bytes are
 * hashed first and shorter keys are zero-padded (FIPS 198-1 section 4). An
 * empty key is accepted. */
TC_status TC_HMAC_SHA384_init(struct TC_HMAC_SHA384_ctx* ctx, TC_bytes key);

/* Absorb message bytes under the update contract. */
TC_status TC_HMAC_SHA384_update(struct TC_HMAC_SHA384_ctx* ctx, TC_bytes data);

/* Write the full TC_SHA384_DIGESTLEN-byte tag and wipe the context. Call init
 * with the key before reuse. */
TC_status TC_HMAC_SHA384_final(struct TC_HMAC_SHA384_ctx* ctx, uint8_t tag[TC_SHA384_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_HMAC_SHA384_ctx_clear(struct TC_HMAC_SHA384_ctx* ctx);

/* One-shot HMAC-SHA-384 truncated to tag.capacity bytes. The default call takes
 * max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)..TC_SHA384_DIGESTLEN; the short-tag
 * call takes 1..that minimum - 1. */
TC_status TC_HMAC_SHA384_digest(TC_bytes key, TC_bytes message, TC_buffer tag);
TC_status TC_HMAC_SHA384_digest_short_tag(TC_bytes key, TC_bytes message, TC_buffer tag);

/* Recompute the tag, truncated to tag.length bytes, and compare in
 * constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR under the verify
 * contract. */
TC_status TC_HMAC_SHA384_verify(TC_bytes key, TC_bytes message, TC_bytes tag);
TC_status TC_HMAC_SHA384_verify_short_tag(TC_bytes key, TC_bytes message, TC_bytes tag);
#endif /* TC_ENABLE_SHA384 */

#if TC_ENABLE_SHA512
/* Key an HMAC-SHA-512 context. Keys longer than TC_SHA512_BLOCKLEN bytes are
 * hashed first and shorter keys are zero-padded (FIPS 198-1 section 4). An
 * empty key is accepted. */
TC_status TC_HMAC_SHA512_init(struct TC_HMAC_SHA512_ctx* ctx, TC_bytes key);

/* Absorb message bytes under the update contract. */
TC_status TC_HMAC_SHA512_update(struct TC_HMAC_SHA512_ctx* ctx, TC_bytes data);

/* Write the full TC_SHA512_DIGESTLEN-byte tag and wipe the context. Call init
 * with the key before reuse. */
TC_status TC_HMAC_SHA512_final(struct TC_HMAC_SHA512_ctx* ctx, uint8_t tag[TC_SHA512_DIGESTLEN]);

/* Wipe the context and leave it inactive. NULL is ignored. */
void TC_HMAC_SHA512_ctx_clear(struct TC_HMAC_SHA512_ctx* ctx);

/* One-shot HMAC-SHA-512 truncated to tag.capacity bytes. The default call takes
 * max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)..TC_SHA512_DIGESTLEN; the short-tag
 * call takes 1..that minimum - 1. */
TC_status TC_HMAC_SHA512_digest(TC_bytes key, TC_bytes message, TC_buffer tag);
TC_status TC_HMAC_SHA512_digest_short_tag(TC_bytes key, TC_bytes message, TC_buffer tag);

/* Recompute the tag, truncated to tag.length bytes, and compare in
 * constant time. Returns TC_OK, TC_MISMATCH or TC_ERROR under the verify
 * contract. */
TC_status TC_HMAC_SHA512_verify(TC_bytes key, TC_bytes message, TC_bytes tag);
TC_status TC_HMAC_SHA512_verify_short_tag(TC_bytes key, TC_bytes message, TC_bytes tag);
#endif /* TC_ENABLE_SHA512 */

#endif /* TC_ENABLE_HMAC */

#ifdef __cplusplus
}
#endif

#endif /* any TC_ENABLE_SHA* */
#endif /* TINY_CRYPTO_HASH_H_ */
