/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TINY_CRYPTO_DES_H_
#define TINY_CRYPTO_DES_H_

#include <tiny_crypto/common.h>

/* DES and TDEA block ciphers with ECB, CBC, CTR, OFB and CFB modes, TDEA-CMAC
 * and the ISO/IEC 9797-1 MAC algorithms 1 and 3.
 * Standards: FIPS 46-3, SP 800-67 Rev. 2, SP 800-38A, SP 800-38B,
 * ISO/IEC 9797-1:2011.
 * Configuration: TC_ENABLE_DES, TC_DES_ENABLE_* per mode, TC_DES_ENABLE_TDES,
 * TC_DES_REJECT_WEAK_KEYS and TC_MIN_TAG_LEN.
 * Limitations: DES has a 56-bit key and its table lookups have no
 * cache-timing protection. Use it for legacy interoperability.
 * After 2023, SP 800-131A Rev. 2 section 2 allows TDEA only for legacy
 * decryption.
 * Work: every function charges no work budget. The functions return
 * TC_status (TC_OK, TC_MISMATCH or TC_ERROR).
 * Contracts: docs/api.md, including its block-mode and DES sections. */

/*
 * Mode selection (define to 1/0 before including this header, or via -D).
 * Default build enables CTR and Triple-DES only. ECB, CBC, CFB*, OFB, and
 * CMAC are opt-in so a build contains code and context fields only for the
 * modes it enables.
 * Only TC_DES_ENABLE_* names are used so this header can co-exist with aes.h.
 */

/* Modes that keep chaining state in ctx->iv */
#if (TC_DES_ENABLE_CBC == 1) || (TC_DES_ENABLE_CTR == 1) || (TC_DES_ENABLE_CFB1 == 1) ||           \
    (TC_DES_ENABLE_CFB8 == 1) || (TC_DES_ENABLE_CFB64 == 1) || (TC_DES_ENABLE_OFB == 1)
#define TC_DES_NEEDS_IV 1
#else
#define TC_DES_NEEDS_IV 0
#endif

#define TC_DES_BLOCKLEN 8 /**< Block length in bytes - DES is a 64-bit (8 bytes) block cipher */
#define TC_DES_KEYLEN 8   /**< Single DES key length in bytes (64 bits total, 56 bits effective) */
/** Two-key TDEA bundle K1 || K2 in bytes. K3 is K1 (112 bits effective). */
#define TC_DES_KEYLEN_2KEY 16
/** Three-key TDEA bundle K1 || K2 || K3 in bytes (168 bits effective). */
#define TC_DES_KEYLEN_3KEY 24

/* Three DES schedules in encrypt, decrypt, encrypt order. */
typedef struct TC_DES_key_bundle {
  uint8_t schedule[48][6];
} TC_DES_key_bundle;

/* Round subkeys held by TC_DES_ctx: one schedule for single DES, three for
 * TDEA when TC_DES_ENABLE_TDES is set. */
#if TC_DES_ENABLE_TDES
#define TC_DES_CTX_SUBKEYS 48
#else
#define TC_DES_CTX_SUBKEYS 16
#endif

/**
 * @brief DES and TDEA context.
 *
 * TC_DES_init selects the cipher from the key length: 8 bytes for single
 * DES, or 16 and 24 bytes for two- and three-key TDEA when TC_DES_ENABLE_TDES
 * is set. TDEA encrypts as E(K1), D(K2), E(K3) (SP 800-67 Rev. 2 section
 * 3). A two-key bundle uses K3 = K1. The context is caller-owned.
 * Fields are private. Clear it with TC_DES_ctx_clear when its lifetime ends.
 */
struct TC_DES_ctx {
  uint8_t schedule[TC_DES_CTX_SUBKEYS][6];
  uint8_t triple; /* 1 when schedule holds a K1, K2, K3 TDEA bundle. */
  uint8_t active;
#if TC_DES_NEEDS_IV
  uint8_t iv[TC_DES_BLOCKLEN];
  uint8_t iv_loaded; /* 1 once TC_DES_set_iv loaded an IV for this key. */
#endif
#if TC_DES_ENABLE_CTR
  uint8_t ctr_stream[TC_DES_BLOCKLEN];
  uint8_t ctr_pos;
  uint8_t ctr_exhausted; /* The counter wrapped. Set a new IV to continue. */
#endif
#if TC_DES_ENABLE_OFB
  uint8_t ofb_pos;
#endif
#if TC_DES_ENABLE_CFB64
  uint8_t cfb64_finished; /* A short segment ended the message. Set a new IV. */
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

#if TC_ENABLE_DES
/* Wipe a DES context (subkeys and IV when present). NULL is a no-op. */
void TC_DES_ctx_clear(struct TC_DES_ctx* ctx);
#endif

/*
 * The ECB, CBC, CTR, CFB and OFB entry points share one argument contract.
 * They return TC_ERROR for a NULL or uninitialized context, a context without
 * an IV from TC_DES_set_iv(every mode except ECB, (TC_bytes){even for an empty buffer, TC_DES_BLOCKLEN}),
 * a NULL buffer with a nonzero length, or a buffer that overlaps the context,
 * and leave the context and buffer unchanged. Otherwise a NULL buffer with
 * length 0 returns TC_OK.
 * Every mode transforms buf in place. The DES cipher itself cannot fail, so
 * a call that passes these checks returns TC_OK unless its own description
 * names another failure.
 */

#if TC_ENABLE_DES
/**
 * @brief Initialize a DES or TDEA context with a key.
 *
 * The context holds no IV afterwards, so the CBC, CTR, CFB and OFB modes
 * return TC_ERROR until TC_DES_set_iv loads one.
 *
 * @param ctx Caller-owned context.
 * @param key Key bytes. They must not overlap ctx.
 * @param keylen TC_DES_KEYLEN, or TC_DES_KEYLEN_2KEY or TC_DES_KEYLEN_3KEY
 *        when TC_DES_ENABLE_TDES is set.
 * @return TC_OK, or TC_ERROR for a NULL argument, another key length, a key
 *         that overlaps ctx, or a key refused under TC_DES_REJECT_WEAK_KEYS.
 *         A NULL ctx is left alone. Every other failure wipes ctx, so a
 *         previous key is unusable after a failed re-init.
 * @note Key parity is ignored. With TC_DES_REJECT_WEAK_KEYS=1 (default 0 for
 *       legacy vectors) weak and semi-weak component keys and TDEA bundles
 *       with K1 = K2 or K2 = K3 are rejected, because those collapse to single
 *       DES. K1 = K3 remains valid two-key TDEA.
 */
TC_status TC_DES_init(struct TC_DES_ctx* ctx, TC_bytes key);
#endif

#if TC_ENABLE_DES && TC_DES_NEEDS_IV
/**
 * @brief Start a new message under the same key.
 *
 * Call it after TC_DES_init and before each new message in an IV mode. It
 * resets the CTR and OFB stream positions, the CTR exhaustion flag and the
 * CFB64 finished flag. Mode calls after it continue that message: CBC and
 * CFB chain from the last ciphertext, and CTR and OFB continue the
 * keystream, so a message may span several calls. The IV stays loaded until
 * the next init or clear.
 *
 * SP 800-38A Appendix C requires an unpredictable CBC and CFB IV and a
 * unique OFB IV per message, and Appendix B unique CTR counter blocks under
 * one key.
 *
 * @param ctx Initialized context.
 * @param iv 8-byte initialization vector, copied into ctx. It must be
 *        disjoint from ctx.
 * @return TC_OK, or TC_ERROR for a NULL argument, an inactive context or an
 *         IV that overlaps ctx. The context is unchanged on error.
 */
TC_status TC_DES_set_iv(struct TC_DES_ctx* ctx, TC_bytes iv);
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_ECB
/**
 * @brief Encrypt one 8-byte block in ECB mode (SP 800-38A section 6.1).
 * ECB leaks equal blocks, so use it only as a building block.
 * @param ctx Initialized context.
 * @param buf 8-byte block, encrypted in place.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_ECB_encrypt(const struct TC_DES_ctx* ctx, TC_buffer buf);

/**
 * @brief Decrypt one 8-byte block in ECB mode.
 * @param ctx Initialized context.
 * @param buf 8-byte block, decrypted in place.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_ECB_decrypt(const struct TC_DES_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_CBC
/**
 * @brief Encrypt a buffer in CBC mode (SP 800-38A section 6.2). The IV
 * carries the chaining value and advances to the last ciphertext block.
 * @param ctx Initialized context.
 * @param buf Data encrypted in place.
 * @param length Data length in bytes, a multiple of 8.
 * @return TC_OK, or TC_ERROR for an argument error or a length that is not
 *         block-aligned (context and buffer unchanged).
 */
TC_status TC_DES_CBC_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf);

/**
 * @brief Decrypt a buffer in CBC mode (SP 800-38A section 6.2). The IV
 * carries the chaining value and advances to the last ciphertext block.
 * @param ctx Initialized context.
 * @param buf Data decrypted in place.
 * @param length Data length in bytes, a multiple of 8.
 * @return TC_OK, or TC_ERROR for an argument error or a length that is not
 *         block-aligned (context and buffer unchanged).
 */
TC_status TC_DES_CBC_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_CTR
/**
 * @brief Encrypt or decrypt a buffer in CTR mode (SP 800-38A section 6.5).
 * Unused keystream bytes serve the next call.
 * @param ctx Initialized context. The IV is the big-endian counter block.
 * @param buf Data of any length, transformed in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error or a request that would
 *         need a block beyond the 2^64-block space of one IV (buffer and
 *         context unchanged). After the counter wraps, calls fail until a
 *         new IV is set.
 */
TC_status TC_DES_CTR_crypt(struct TC_DES_ctx* ctx, TC_buffer buf);
#endif

#if TC_DES_ENABLE_CFB64
/*
 * CFB64 (NIST SP 800-38A section 6.3, s = 64) processes whole 8-byte
 * segments, as section 5.2 requires. As an extension, a call may end with one
 * short segment of 1..7 bytes. That segment shifts only its ciphertext bytes
 * into the feedback register and finishes the message. Later CFB64 calls
 * return TC_ERROR until TC_DES_set_iv or an init starts a new message.
 * Split a message at multiples of 8 bytes to get the same output as one call.
 */

#if TC_ENABLE_DES
/**
 * @brief Encrypt a buffer in CFB64 mode.
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Data encrypted in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error or a finished message.
 *         The context and buffer are unchanged on error.
 */
TC_status TC_DES_CFB64_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf);

/**
 * @brief Decrypt a buffer in CFB64 mode.
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Data decrypted in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error or a finished message.
 *         The context and buffer are unchanged on error.
 */
TC_status TC_DES_CFB64_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf);
#endif
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_CFB8
/**
 * @brief Encrypt a buffer in CFB8 mode (SP 800-38A section 6.3, s = 8).
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Data of any length, encrypted in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_CFB8_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf);

/**
 * @brief Decrypt a buffer in CFB8 mode (SP 800-38A section 6.3, s = 8).
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Data of any length, decrypted in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_CFB8_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_CFB1
/**
 * @brief Encrypt bits in CFB1 mode (SP 800-38A section 6.3, s = 1).
 *
 * Bits are packed MSB-first: bit i of the stream is (buf[i/8] >> (7 - i%8)) & 1.
 * Trailing pad bits of the final byte are left unchanged.
 *
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Packed bit buffer of at least ceil(bit_length / 8) bytes, all
 *        disjoint from ctx.
 * @param bit_length Data length in bits.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_CFB1_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf, size_t bit_length);

/**
 * @brief Decrypt bits in CFB1 mode, packed as for TC_DES_CFB1_encrypt.
 * @param ctx Initialized context (IV holds the feedback register).
 * @param buf Packed bit buffer of at least ceil(bit_length / 8) bytes.
 * @param bit_length Data length in bits.
 * @return TC_OK, or TC_ERROR for an argument error.
 */
TC_status TC_DES_CFB1_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf, size_t bit_length);
#endif

#if TC_ENABLE_DES && TC_DES_ENABLE_OFB
/**
 * @brief Encrypt or decrypt a buffer in OFB mode (SP 800-38A section 6.4).
 * Unused output-block bytes serve the next call.
 * @param ctx Initialized context (IV holds the output feedback block).
 * @param buf Data of any length, transformed in place.
 * @param length Data length in bytes.
 * @return TC_OK, or TC_ERROR for an argument error or a corrupted stream
 *         position.
 */
TC_status TC_DES_OFB_crypt(struct TC_DES_ctx* ctx, TC_buffer buf);
#endif

/* --- DES / 3DES CMAC (NIST SP 800-38B) --- */
#if TC_DES_ENABLE_CMAC

/* Full CMAC tag is one DES block. Shorter tags are the leading tag_len bytes. */
#define TC_DES_CMAC_TAG_MAX TC_DES_BLOCKLEN

#if TC_ENABLE_DES
/*
 * DES/3DES-CMAC (NIST SP 800-38B). One-shot.
 * keylen must be 8 (single DES), 16 (2-key TDEA), or 24 (3-key TDEA).
 * tag_len must be in TC_MIN_TAG_LEN..TC_DES_CMAC_TAG_MAX. Truncation keeps
 * the most significant octets of the full T (SP 800-38B section 6.2).
 * Empty message: msg may be NULL when msg_len is 0. The tag is written
 * last, so it may overlap key or msg. The key schedules and the full tag on
 * the stack are wiped before return.
 * @return TC_OK, or TC_ERROR for a NULL key or tag, a bad key length, a key
 *         refused under TC_DES_REJECT_WEAK_KEYS, a NULL msg with a nonzero
 *         length or a tag length outside the range. Every failure leaves tag
 *         unchanged.
 */
TC_status TC_DES_CMAC(TC_bytes key, TC_bytes msg, TC_buffer tag);

/* Recompute the CMAC and compare tag_len bytes in constant time
 * (SP 800-38B section 6.3), with the same length range as TC_DES_CMAC.
 * @return TC_OK when the tag matches, TC_MISMATCH when it differs, or
 *         TC_ERROR as for TC_DES_CMAC. */
TC_status TC_DES_CMAC_verify(TC_bytes key, TC_bytes msg, TC_bytes tag);

/* Short-tag DES/3DES-CMAC and verify. tag_len must be in
 * 1..TC_MIN_TAG_LEN - 1. Use them only when the protocol fixes the short tag
 * and limits failed verifications for the key (SP 800-38B Appendix A.2).
 * Status values follow TC_DES_CMAC and TC_DES_CMAC_verify. */
TC_status TC_DES_CMAC_short_tag(TC_bytes key, TC_bytes msg, TC_buffer tag);
TC_status TC_DES_CMAC_verify_short_tag(TC_bytes key, TC_bytes msg, TC_bytes tag);
#endif

/*
 * Streaming DES/3DES-CMAC (SP 800-38B sections 6.1 and 6.2). Holds its own
 * key schedules so it works with the ECB/CBC/TDES mode gates compiled out.
 * The most recent block is held back in buf so *_final can apply K1
 * (complete) or K2 (padded) to the true last block. *_final always emits
 * the full TC_DES_CMAC_TAG_MAX bytes. Argument errors leave the context
 * unchanged. Otherwise *_final wipes it, on success and on failure. Call
 * *_init again before reuse.
 */
struct TC_DES_CMAC_ctx {
  TC_DES_key_bundle keys;
  uint8_t k1[TC_DES_BLOCKLEN];
  uint8_t k2[TC_DES_BLOCKLEN];
  uint8_t mac[TC_DES_BLOCKLEN];
  uint8_t buf[TC_DES_BLOCKLEN];
  uint8_t buf_len;
  uint8_t triple;
  uint8_t active;
};

#if TC_ENABLE_DES
/* init keys ctx and derives the subkeys. keylen must be TC_DES_KEYLEN,
 * TC_DES_KEYLEN_2KEY (K1, K2, K1) or TC_DES_KEYLEN_3KEY, and key must be
 * disjoint from ctx. It returns TC_OK, or TC_ERROR for a NULL argument,
 * another key length, an overlap or a key refused under
 * TC_DES_REJECT_WEAK_KEYS. A NULL ctx is left alone, and every other failure
 * leaves ctx wiped.
 * update absorbs data, which may be NULL when len is 0. It returns TC_OK, or
 * TC_ERROR with ctx unchanged for a NULL ctx, a NULL data with a nonzero len,
 * data that overlaps ctx, or an unkeyed ctx.
 * final writes the 8-byte tag, which must be disjoint from ctx. It returns
 * TC_OK, or TC_ERROR with ctx unchanged for a NULL argument, an overlap or an
 * unkeyed ctx. Otherwise final wipes ctx.
 * ctx_clear wipes ctx and ignores NULL. */
TC_status TC_DES_CMAC_init(struct TC_DES_CMAC_ctx* ctx, TC_bytes key);
TC_status TC_DES_CMAC_update(struct TC_DES_CMAC_ctx* ctx, TC_bytes data);
TC_status TC_DES_CMAC_final(struct TC_DES_CMAC_ctx* ctx, TC_buffer tag);
void TC_DES_CMAC_ctx_clear(struct TC_DES_CMAC_ctx* ctx);
#endif

#endif /* TC_DES_ENABLE_CMAC */

#if TC_DES_ENABLE_ISO9797
/*
 * ISO/IEC 9797-1:2011 MAC algorithm 1 (CBC-MAC) or 3 (retail MAC) over DES.
 *
 * Algorithm 1 runs CBC under a 16- or 24-byte TDEA bundle. ISO/IEC
 * 9797-1:2011 clause 5 restricts single DES to Algorithms 3 and 4.
 * Algorithm 3 (clause 7.4) runs CBC under single-DES K1 and applies Output
 * Transformation 3 (clause 6.7.4) as D(K2) then E(K1). A 24-byte key K1 || K2
 * || K3 selects the three-key retail extension, whose final encryption uses
 * K3. That extension is outside ISO/IEC 9797-1.
 *
 * Padding 1 (clause 6.3.2) adds zero bytes only to a partial block. Padding
 * 2 (clause 6.3.3) always adds 0x80 followed by zeroes. NONE requires block
 * alignment and a nonempty message. Padding 1 on an empty message processes
 * one zero block. The caller must authenticate a fixed or separately
 * authenticated length when using NONE or padding 1.
 *
 * Algorithm 3 requires a 16-byte K1 || K2 bundle. The separately named
 * three-key extension requires 24 bytes. Both reject parity-equivalent
 * adjacent component keys in every build because they collapse a DES stage.
 * TC_DES_REJECT_WEAK_KEYS additionally rejects weak and semi-weak components.
 *
 * The context is caller-owned and final consumes it. Input, key and tag
 * buffers must be disjoint from the context. Overlap is an argument error. A
 * failed init wipes the context. Every failed final leaves the tag
 * untouched. An argument error in update or final leaves the context
 * unchanged. Any other failure wipes it.
 */
typedef enum TC_DES_ISO9797_algorithm {
  TC_DES_ISO9797_ALG1 = 1,
  TC_DES_ISO9797_ALG3 = 3,
  TC_DES_ISO9797_ALG3_3KEY_EXTENSION = 0x103
} TC_DES_ISO9797_algorithm;

typedef enum TC_DES_ISO9797_padding {
  TC_DES_ISO9797_PAD_NONE = 0,
  TC_DES_ISO9797_PAD1 = 1,
  TC_DES_ISO9797_PAD2 = 2
} TC_DES_ISO9797_padding;

struct TC_DES_ISO9797_ctx {
  TC_DES_key_bundle keys;
  uint8_t mac[TC_DES_BLOCKLEN];
  uint8_t buf[TC_DES_BLOCKLEN];
  uint8_t used;
  uint16_t algorithm;
  uint8_t padding;
  uint8_t active;
  uint8_t nonempty;
};

#if TC_ENABLE_DES
/* Algorithm 1 takes a 16- or 24-byte TDEA bundle, Algorithm 3 takes exactly
 * 16 bytes, and ALG3_3KEY_EXTENSION takes exactly 24 bytes.
 * Returns TC_OK, or TC_ERROR for a NULL argument, an unknown algorithm or
 * padding, another key length, a key that overlaps ctx or a key refused
 * under TC_DES_REJECT_WEAK_KEYS. A NULL ctx is left alone. */
TC_status TC_DES_ISO9797_init(struct TC_DES_ISO9797_ctx* ctx, TC_DES_ISO9797_algorithm algorithm,
                              TC_DES_ISO9797_padding padding, TC_bytes key);
/* Absorb msg, which may be NULL when msg_len is 0. Returns TC_OK, or
 * TC_ERROR with ctx unchanged for a NULL ctx, a NULL msg with a nonzero
 * length, msg that overlaps ctx, or an inactive or corrupted ctx. */
TC_status TC_DES_ISO9797_update(struct TC_DES_ISO9797_ctx* ctx, TC_bytes msg);
/* Pad, apply the output transformation and write the full 8-byte MAC.
 * Callers truncate it to leading bytes (clause 6.8). Returns TC_OK, or
 * TC_ERROR with ctx unchanged for a NULL argument, a tag that overlaps ctx,
 * or an inactive or corrupted ctx. A NONE-padded message that is empty or
 * unaligned returns TC_ERROR and wipes ctx. Every other return wipes ctx. */
TC_status TC_DES_ISO9797_final(struct TC_DES_ISO9797_ctx* ctx, TC_buffer tag);
/* Wipe the context. NULL is ignored. */
void TC_DES_ISO9797_ctx_clear(struct TC_DES_ISO9797_ctx* ctx);
/* One-shot MAC and verify over the streaming functions. The default forms
 * require tag_len 8. The tag is written last, so it may overlap key or msg.
 * MAC returns TC_OK, or TC_ERROR with tag untouched for a NULL tag, a NULL
 * msg with a nonzero length, another tag length, or any init or final error.
 * Verify compares in constant time and returns TC_OK on a match,
 * TC_MISMATCH for a different tag and TC_ERROR for the MAC errors. */
TC_status TC_DES_ISO9797_MAC(TC_DES_ISO9797_algorithm algorithm, TC_DES_ISO9797_padding padding,
                             TC_bytes key, TC_bytes msg, TC_buffer tag);
TC_status TC_DES_ISO9797_verify(TC_DES_ISO9797_algorithm algorithm, TC_DES_ISO9797_padding padding,
                                TC_bytes key, TC_bytes msg, TC_bytes tag);
/* Explicit truncated-MAC API (clause 6.8). Accepts the leading 4..7 bytes
 * and otherwise follows the default forms. */
TC_status TC_DES_ISO9797_MAC_short_tag(TC_DES_ISO9797_algorithm algorithm,
                                       TC_DES_ISO9797_padding padding, TC_bytes key, TC_bytes msg,
                                       TC_buffer tag);
TC_status TC_DES_ISO9797_verify_short_tag(TC_DES_ISO9797_algorithm algorithm,
                                          TC_DES_ISO9797_padding padding, TC_bytes key,
                                          TC_bytes msg, TC_bytes tag);
#endif
#endif /* TC_DES_ENABLE_ISO9797 */

#ifdef __cplusplus
}
#endif

#endif /* TINY_CRYPTO_DES_H_ */
