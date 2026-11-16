/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* AES block cipher and modes: CTR, CBC, ECB, OFB, GCM, CCM, EAX, EAX', SIV
 * and CMAC with a key size fixed by TC_AES_KEY_BITS.
 * Standards: FIPS 197, SP 800-38A (CTR, CBC, ECB, OFB), SP 800-38B (CMAC),
 * SP 800-38C (CCM), SP 800-38D (GCM), RFC 5297 (SIV), ANSI C12.22 (EAX').
 * Configuration: TC_ENABLE_AES, TC_AES_KEY_BITS, TC_AES_ENABLE_* per mode,
 * TC_AES_SBOX_MODE, TC_AES_GCM_GHASH_MODE, TC_AES_TINY and TC_MIN_TAG_LEN.
 * Limitations: GCM streams encryption only. The fast S-box and fast-table
 * GHASH have no cache-timing protection.
 * Work: every function charges no work budget. The functions return
 * TC_status (TC_OK, TC_MISMATCH or TC_ERROR).
 * Contracts: docs/api.md, including its AEAD, tag-length and block-mode rules. */
#ifndef TINY_CRYPTO_AES_H_
#define TINY_CRYPTO_AES_H_

#include <tiny_crypto/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mode selection (define to 1/0 before including this header, or via -D).
 * Default build enables CTR only. CBC, ECB, OFB, CCM, EAX, EAX_PRIME, GCM,
 * SIV, and CMAC are opt-in so a build contains code and context fields only for
 * the modes it enables.
 */

#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_HARDWARE
/* Platform hook required by the hardware GHASH profile. The application
 * defines it. It writes the GF(2^128) product left * right of
 * SP 800-38D section 6.3 to result, and it must succeed. The library passes
 * three distinct arrays. right is the hash subkey H. The hook must keep no
 * copy of its inputs after it returns. */
void TC_AES_GCM_hardware_multiply(uint8_t result[16], const uint8_t left[16],
                                  const uint8_t right[16]);
#endif

/* Modes that keep an IV in TC_AES_ctx. */
#define TC_AES_HAVE_IV (TC_AES_ENABLE_CBC || TC_AES_ENABLE_CTR || TC_AES_ENABLE_OFB)

#define TC_AES_BLOCKLEN 16 /* AES block length in bytes (128-bit block only). */

#if TC_AES_KEY_BITS == 256
#define TC_AES_KEYLEN 32
#define TC_AES_KEY_EXP_SIZE 240
#elif TC_AES_KEY_BITS == 192
#define TC_AES_KEYLEN 24
#define TC_AES_KEY_EXP_SIZE 208
#else
#define TC_AES_KEYLEN 16
#define TC_AES_KEY_EXP_SIZE 176
#endif

struct TC_AES_key_ctx {
  uint8_t round_key[TC_AES_KEY_EXP_SIZE];
  uint8_t active;
};

struct TC_AES_ctx {
  struct TC_AES_key_ctx key;
#if TC_AES_HAVE_IV
  uint8_t iv[TC_AES_BLOCKLEN];
  uint8_t iv_loaded; /* 1 once TC_AES_set_iv loaded an IV for this key. */
#if TC_AES_ENABLE_CTR
  uint8_t ctr_stream[TC_AES_BLOCKLEN];
  uint8_t ctr_pos;
  uint8_t ctr_exhausted; /* The counter wrapped. Set a new IV to continue. */
#endif
#if TC_AES_ENABLE_OFB
  uint8_t ofb_pos;
#endif
#endif
};

#if TC_ENABLE_AES
/* Expand a TC_AES_KEYLEN-byte key into the key schedule used by ECB
 * (FIPS 197 section 5.2). key must be disjoint from ctx.
 * Returns TC_OK, or TC_ERROR for a NULL argument, a key that overlaps ctx,
 * or, in the runtime S-box profile, a call before TC_AES_init_sbox. A NULL
 * ctx is left alone. Every other failure wipes ctx, so a previous key is
 * unusable after a failed re-init. */
TC_status TC_AES_key_init(struct TC_AES_key_ctx* ctx, TC_bytes key);
/* Wipe a key schedule. NULL is ignored. */
void TC_AES_key_ctx_clear(struct TC_AES_key_ctx* ctx);

/* Wipe an AES context, including the IV and streaming state. NULL is
 * ignored. */
void TC_AES_ctx_clear(struct TC_AES_ctx* ctx);

/* Key an AES context with TC_AES_KEYLEN bytes (FIPS 197 section 5.2). The
 * context holds no IV afterwards, so CBC, CTR and OFB return TC_ERROR until
 * TC_AES_set_iv loads one. key must be disjoint from ctx.
 * Returns TC_OK, or TC_ERROR under the TC_AES_key_init conditions. A NULL
 * ctx is left alone. Every other failure wipes ctx. */
TC_status TC_AES_init(struct TC_AES_ctx* ctx, TC_bytes key);
#endif
#if TC_ENABLE_AES && TC_AES_CAVP
/* Test-only single-block hooks used by the AESAVS harness. They return
 * TC_ERROR, leaving block unchanged, when the key cannot be scheduled or
 * block is not TC_AES_BLOCKLEN bytes of storage. */
TC_status TC_AES_CAVP_encrypt_block(TC_bytes key, TC_buffer block);
TC_status TC_AES_CAVP_decrypt_block(TC_bytes key, TC_buffer block);
#endif
#if TC_ENABLE_AES && TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
/* Build the S-boxes in RAM. Call it once before any key init. Until then,
 * every key init returns TC_ERROR. Each call rebuilds the tables with the
 * same values. A call writes the shared tables, so make it before sharing
 * AES across threads and never while another thread uses AES. */
void TC_AES_init_sbox(void);
#endif
#if TC_ENABLE_AES && TC_AES_HAVE_IV
/* Start a message: copy a 16-byte IV into ctx and reset the CTR and OFB
 * stream state. Call it after TC_AES_init and before each new message. Mode
 * calls after it continue that message: CBC chains from the last ciphertext
 * block, and CTR and OFB continue the keystream, so a message may span
 * several calls. The IV stays loaded until the next init or clear.
 * SP 800-38A Appendix C requires an unpredictable CBC IV and a unique OFB IV
 * per message, and Appendix B unique CTR counter blocks under one key.
 * Returns TC_OK, or TC_ERROR with ctx unchanged for a NULL argument, a
 * context without a key or an IV that overlaps ctx. */
TC_status TC_AES_set_iv(struct TC_AES_ctx* ctx, TC_bytes iv);
#endif

/*
 * The CBC, CTR, OFB and ECB functions below transform buf in place and share
 * one failure rule. They return TC_OK, or TC_ERROR with buf and ctx
 * unchanged for a NULL or unkeyed context, a context without an IV from
 * TC_AES_set_iv (CBC, CTR and OFB, even for an empty buf), a NULL buf with a
 * nonzero length, or a buf that overlaps ctx. An empty buf returns TC_OK. A
 * block cipher failure part way through wipes buf and clears ctx, so
 * neither partial output nor a broken chaining value survives. ECB takes a
 * const key schedule, so its failure wipes buf only.
 */

#if TC_ENABLE_AES && TC_AES_ENABLE_ECB
/* Encrypt or decrypt one TC_AES_BLOCKLEN-byte block in place (FIPS 197
 * sections 5.1 and 5.3, SP 800-38A section 6.1). ECB leaks equal blocks,
 * so use it only as a building block. */
TC_status TC_AES_ECB_encrypt(const struct TC_AES_key_ctx* ctx, TC_buffer buf);
TC_status TC_AES_ECB_decrypt(const struct TC_AES_key_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_AES && TC_AES_ENABLE_CBC
/*
 * CBC (SP 800-38A section 6.2). length must be a multiple of
 * TC_AES_BLOCKLEN, and an unaligned length is an argument error. The caller
 * applies padding. The IV in ctx advances to the last ciphertext block, so
 * the next call continues the message.
 */
TC_status TC_AES_CBC_encrypt(struct TC_AES_ctx* ctx, TC_buffer buf);
TC_status TC_AES_CBC_decrypt(struct TC_AES_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_AES && TC_AES_ENABLE_CTR
/*
 * CTR (SP 800-38A section 6.5). Encrypt and decrypt are the same operation.
 * The IV is the big-endian counter block. It increments for every block, and
 * one IV covers at most 2^128 blocks across all calls. Returns TC_ERROR, with
 * buf and ctx unchanged, when the request would need a block beyond that
 * space. Once the counter wraps, further calls fail until TC_AES_set_iv
 * supplies a new IV. Unused keystream bytes serve the next call.
 */
TC_status TC_AES_CTR_crypt(struct TC_AES_ctx* ctx, TC_buffer buf);
#endif

#if TC_ENABLE_AES && TC_AES_ENABLE_OFB
/*
 * OFB (SP 800-38A section 6.4). Encrypt and decrypt are the same operation.
 * Unused output-block bytes serve the next call. OFB provides
 * confidentiality only. A context whose stream position is corrupted
 * returns TC_ERROR with buf unchanged.
 */
TC_status TC_AES_OFB_crypt(struct TC_AES_ctx* ctx, TC_buffer buf);
#endif

/*
 * One-shot AEAD contract (GCM, CCM, EAX, EAX', SIV). docs/api.md has the
 * full description.
 * - Encrypt writes plaintext.length bytes to ciphertext and tag.capacity tag
 *   bytes to tag. Decrypt writes ciphertext.length bytes to plaintext and
 *   checks tag.length tag bytes. The tag length is fixed for the key.
 *   EAX' and SIV take fixed-size tag arrays.
 * - CCM and EAX default entry points take tags of at least TC_MIN_TAG_LEN
 *   bytes (config.h, default 8). Their _short_tag forms take only the
 *   shorter lengths, for protocols that bound failed verifications
 *   (SP 800-38B Appendix A.2). GCM short tags follow SP 800-38D Appendix C.
 * - The text output capacity must be at least the text input length.
 * - Text input and output are exact aliases or fully disjoint. The tag is
 *   disjoint from the text output. SIV associated data and its descriptor
 *   array are disjoint from the text output. A violation returns TC_ERROR before a write.
 * - The key, and the nonce and AAD of GCM, CCM, EAX and EAX', may share
 *   storage with the text output. Each is read in full before the first
 *   output write.
 * - The caller keeps the key, nonce, AAD, text input and received tag stable
 *   for the duration of the call. GCM, CCM, EAX and EAX' decrypt read the
 *   ciphertext twice, once to authenticate it and once to decrypt it.
 * - TC_ERROR before any write: NULL key, NULL tag, NULL text or AAD with a
 *   nonzero length, short text output, a forbidden overlap, a nonce or tag
 *   length outside the mode's range, or a length above the mode's limit.
 * - GCM, CCM, EAX and EAX' decrypt authenticate before writing plaintext.
 *   SIV decrypt writes candidate plaintext and then recomputes the synthetic
 *   IV over the AD and that plaintext (RFC 5297 section 2.7).
 * - After the argument checks, every failure wipes input.length bytes of the
 *   text output: TC_MISMATCH for a tag that fails to verify and TC_ERROR for
 *   a cipher backend failure. In-place callers lose the input. The tag output
 *   is written only on success.
 */

#if TC_AES_ENABLE_GCM

/*
 * NIST SP 800-38D length limits (bit lengths converted to bytes):
 *   1 <= len(IV) <= 2^64-1 bits
 *   len(P) <= 2^39-256 bits  =>  TC_AES_GCM_MAX_PLAINTEXT_BYTES
 *   len(A) <= 2^64-1 bits
 * Tag length t (bits) is one of 128,120,112,104,96,64,32 and is fixed for the
 * key for the life of this context (passed at init).
 *
 * Appendix C short-tag packet limits (most permissive table row):
 *   t=32: |C|+|A| <= 2^10 bytes per packet
 *   t=64: |C|+|A| <= 2^25 bytes per packet
 * Key lifetime / max decryption invocations remain the application's duty.
 */
#define TC_AES_GCM_MAX_PLAINTEXT_BYTES ((((uint64_t)1) << 36) - 32u)
#define TC_AES_GCM_MAX_AAD_BYTES (UINT64_MAX / 8u)
#define TC_AES_GCM_MAX_IV_BYTES (UINT64_MAX / 8u)
#define TC_AES_GCM_SHORT_TAG4_MAX_PACKET ((uint64_t)1 << 10) /* 1024 */
#define TC_AES_GCM_SHORT_TAG8_MAX_PACKET ((uint64_t)1 << 25) /* 33554432 */

struct TC_AES_GCM_ctx {
  struct TC_AES_key_ctx key;
  uint8_t h[TC_AES_BLOCKLEN];
  uint8_t j0[TC_AES_BLOCKLEN];
  uint8_t counter[TC_AES_BLOCKLEN];
  uint8_t stream[TC_AES_BLOCKLEN];
  uint8_t s[TC_AES_BLOCKLEN];
  uint8_t ghash[TC_AES_BLOCKLEN];
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  uint8_t ghash_table[16][TC_AES_BLOCKLEN];
#endif

  uint64_t aad_len;
  uint64_t text_len;
  uint8_t stream_pos;
  uint8_t ghash_len;
  uint8_t tag_len; /* fixed for this key/context (SP 800-38D §5.2.1.2) */
  uint8_t phase;
};

#if TC_ENABLE_AES
/*
 * Streaming GCM encryption (SP 800-38D section 7.1). Decryption is one-shot
 * (TC_AES_GCM_decrypt), so the tag is verified before any plaintext is
 * released. The calls run init, aad_update, encrypt_update, then
 * encrypt_finish.
 *
 * init keys ctx and fixes tag_len for the message: 12..16 bytes, or 4 or 8
 * bytes through the short-tag initializer under the Appendix C packet
 * limits. iv.length is 1..TC_AES_GCM_MAX_IV_BYTES, and 12 bytes is the fast
 * path (section 7.1 step 2). key and iv must be disjoint from ctx and are
 * read only during init. Never reuse an IV with the same key (section 8).
 * Returns TC_OK, or TC_ERROR for a NULL argument, an empty or oversized IV,
 * a tag length outside the initializer's set, a key or IV that overlaps ctx,
 * or a key init failure. A NULL ctx is left alone. Every other failure wipes
 * ctx.
 */
TC_status TC_AES_GCM_init(struct TC_AES_GCM_ctx* ctx, TC_bytes key, TC_bytes iv, size_t tag_len);
TC_status TC_AES_GCM_init_short_tag(struct TC_AES_GCM_ctx* ctx, TC_bytes key, TC_bytes iv,
                                    size_t tag_len);

/* aad_update absorbs AAD, and encrypt_update encrypts buf in place and
 * absorbs the ciphertext. Supply all AAD first. An aad_update after the
 * first encrypt_update returns TC_ERROR. aad and buf must be disjoint from
 * ctx. A zero length accepts a NULL pointer.
 * Each returns TC_OK, or TC_ERROR with buf and ctx unchanged for a NULL,
 * uninitialized or finished context, a NULL pointer with a nonzero length,
 * an overlap, or a total above TC_AES_GCM_MAX_AAD_BYTES,
 * TC_AES_GCM_MAX_PLAINTEXT_BYTES or the short-tag packet limit. A cipher
 * backend failure wipes buf, clears ctx and returns TC_ERROR. */
TC_status TC_AES_GCM_aad_update(struct TC_AES_GCM_ctx* ctx, TC_bytes aad);
TC_status TC_AES_GCM_encrypt_update(struct TC_AES_GCM_ctx* ctx, TC_buffer buf);

/* Write the tag_len-byte tag chosen at init (section 7.1 steps 5 and 6).
 * tag must be disjoint from ctx. Returns TC_OK, or TC_ERROR with tag and ctx
 * unchanged for a NULL argument, an overlap, or a context that was never
 * initialized or is finished. Otherwise finish consumes ctx and wipes it,
 * and a cipher backend failure returns TC_ERROR with tag unchanged. */
TC_status TC_AES_GCM_encrypt_finish(struct TC_AES_GCM_ctx* ctx, TC_buffer tag);

/* One-shot GCM (SP 800-38D sections 7.1 and 7.2). Follows the one-shot
 * AEAD contract above. iv.length is 1..TC_AES_GCM_MAX_IV_BYTES. The default
 * forms take 12..16-byte tags and the _short_tag forms 4 or 8 bytes, under
 * the Appendix C packet limits. Text above TC_AES_GCM_MAX_PLAINTEXT_BYTES
 * returns TC_ERROR. */
TC_status TC_AES_GCM_encrypt(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_GCM_decrypt(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext);
TC_status TC_AES_GCM_encrypt_short_tag(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes plaintext,
                                       TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_GCM_decrypt_short_tag(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes ciphertext,
                                       TC_bytes tag, TC_buffer plaintext);

/* Wipe the key schedule, hash subkey and authentication state. NULL is
 * ignored. */
void TC_AES_GCM_ctx_clear(struct TC_AES_GCM_ctx* ctx);
#endif

#endif /* TC_AES_ENABLE_GCM */

#if TC_ENABLE_AES && TC_AES_ENABLE_CCM

/* CCM (SP 800-38C sections 6.1 and 6.2) is a packet mode. Payload and AAD
 * lengths are known at entry. Follows the one-shot AEAD contract above. The
 * nonce is 7..13 bytes (Appendix A.1). CCM tags have an even length of
 * 4..16 bytes. The default entry points take those of at least
 * TC_MIN_TAG_LEN (8, 10, 12, 14 or 16 by default). The _short_tag forms take
 * those below it (4 or 6 by default). Any other tag length returns TC_ERROR.
 * The payload length must fit the 15 - nonce length byte length field. */
TC_status TC_AES_CCM_encrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_CCM_decrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext);
TC_status TC_AES_CCM_encrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes plaintext, TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_CCM_decrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes ciphertext, TC_bytes tag, TC_buffer plaintext);

#endif

#if TC_ENABLE_AES && TC_AES_ENABLE_EAX

/* EAX one-shot AEAD (Bellare, Rogaway and Wagner, "The EAX Mode of
 * Operation"). Follows the one-shot AEAD contract above. The tag is the
 * leading bytes of the 16-byte EAX tag. The default entry points take
 * TC_MIN_TAG_LEN..16 bytes. The _short_tag forms take 1..TC_MIN_TAG_LEN - 1
 * bytes. Other tag lengths, including 0, return TC_ERROR. The nonce may have
 * any length, including zero. */
TC_status TC_AES_EAX_encrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_EAX_decrypt(TC_bytes key, TC_bytes nonce, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext);
TC_status TC_AES_EAX_encrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes plaintext, TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_EAX_decrypt_short_tag(TC_bytes key, TC_bytes nonce, TC_bytes aad,
                                       TC_bytes ciphertext, TC_bytes tag, TC_buffer plaintext);

#endif

#if TC_AES_ENABLE_EAX_PRIME

#define TC_AES_EAX_PRIME_TAG_LEN 4

#if TC_ENABLE_AES
/* ANSI C12.22 EAX'. Follows the one-shot AEAD contract above. The protocol
 * fixes the tag at four bytes, so EAX' is exempt from TC_MIN_TAG_LEN and has
 * no _short_tag form. The cleartext header is authenticated and serves as
 * the nonce. A NULL tag returns TC_ERROR. */
TC_status TC_AES_EAX_PRIME_encrypt(TC_bytes key, TC_bytes cleartext, TC_bytes plaintext,
                                   TC_buffer ciphertext, TC_buffer tag);
TC_status TC_AES_EAX_PRIME_decrypt(TC_bytes key, TC_bytes cleartext, TC_bytes ciphertext,
                                   TC_bytes tag, TC_buffer plaintext);
#endif

#endif

#if TC_AES_ENABLE_CMAC

/* Full CMAC tag is one AES block. Shorter tags are the leading tag_len bytes. */
#define TC_AES_CMAC_TAG_MAX TC_AES_BLOCKLEN

#if TC_ENABLE_AES
/*
 * One-shot AES-CMAC (SP 800-38B section 6.2) over a TC_AES_KEYLEN-byte key.
 * tag_len must be in TC_MIN_TAG_LEN..TC_AES_CMAC_TAG_MAX. Truncation keeps
 * the most significant octets of the full T (section 6.2 step 7). msg may be
 * NULL when msg_len is 0. The tag is written last, so it may overlap key or
 * msg. The context and full tag on the stack are wiped before return.
 * @return TC_OK, or TC_ERROR for a NULL key or tag, a NULL msg with a nonzero
 *         length, a tag length outside the range, or a key init or cipher
 *         failure. Every failure leaves tag unchanged.
 */
TC_status TC_AES_CMAC(TC_bytes key, TC_bytes msg, TC_buffer tag);

/* Recompute the CMAC and compare tag_len bytes in constant time
 * (SP 800-38B section 6.3), with the same length range as TC_AES_CMAC.
 * @return TC_OK when the tag matches, TC_MISMATCH when it differs, or
 *         TC_ERROR as for TC_AES_CMAC. */
TC_status TC_AES_CMAC_verify(TC_bytes key, TC_bytes msg, TC_bytes tag);

/* Short-tag AES-CMAC and verify. tag_len must be in 1..TC_MIN_TAG_LEN - 1.
 * Use them only when the protocol fixes the short tag and limits failed
 * verifications for the key (SP 800-38B Appendix A.2). Status values follow
 * TC_AES_CMAC and TC_AES_CMAC_verify. */
TC_status TC_AES_CMAC_short_tag(TC_bytes key, TC_bytes msg, TC_buffer tag);
TC_status TC_AES_CMAC_verify_short_tag(TC_bytes key, TC_bytes msg, TC_bytes tag);
#endif

/*
 * Streaming AES-CMAC (SP 800-38B sections 6.1 and 6.2). The most recent
 * block is held back in buf so that *_final can apply K1 (complete) or K2
 * (padded) to the true last block. *_final always emits the full
 * TC_AES_CMAC_TAG_MAX bytes. Callers may truncate the tag. Argument errors
 * leave the context unchanged. Otherwise *_final wipes it, on success and on
 * failure. Call *_init again before reuse.
 */
struct TC_AES_CMAC_ctx {
  struct TC_AES_key_ctx key;
  uint8_t k1[TC_AES_BLOCKLEN];
  uint8_t k2[TC_AES_BLOCKLEN];
  uint8_t mac[TC_AES_BLOCKLEN];
  uint8_t buf[TC_AES_BLOCKLEN];
  uint8_t buf_len;
  uint8_t active;
};

#if TC_ENABLE_AES
/* init keys ctx and derives the subkeys. key is TC_AES_KEYLEN bytes and
 * must be disjoint from ctx. It returns TC_OK, or TC_ERROR for a NULL
 * argument, an overlap or a key init failure. A NULL ctx is left alone, and
 * every other failure leaves ctx wiped.
 * update absorbs data, which may be NULL when len is 0. It returns TC_OK, or
 * TC_ERROR with ctx unchanged for a NULL ctx, a NULL data with a nonzero len,
 * data that overlaps ctx, or a context without a key. A cipher failure
 * clears ctx.
 * final writes the 16-byte tag, which must be disjoint from ctx. It returns
 * TC_OK, or TC_ERROR with ctx unchanged for a NULL argument, an overlap or a
 * context without a key. Otherwise final wipes ctx.
 * ctx_clear wipes ctx and ignores NULL. */
TC_status TC_AES_CMAC_init(struct TC_AES_CMAC_ctx* ctx, TC_bytes key);
TC_status TC_AES_CMAC_update(struct TC_AES_CMAC_ctx* ctx, TC_bytes data);
TC_status TC_AES_CMAC_final(struct TC_AES_CMAC_ctx* ctx, TC_buffer tag);
void TC_AES_CMAC_ctx_clear(struct TC_AES_CMAC_ctx* ctx);
#endif

#endif

#if TC_AES_ENABLE_SIV

/* RFC 5297 SIV-AES: key is two equal AES keys concatenated (CMAC || CTR). */
#define TC_AES_SIV_KEYLEN (TC_AES_KEYLEN * 2)
#define TC_AES_SIV_V_LEN TC_AES_BLOCKLEN
/* RFC §7: at most 126 associated-data components (plaintext is the last S2V input). */
#define TC_AES_SIV_MAX_AD 126u

#if TC_ENABLE_AES
/*
 * One-shot SIV (RFC 5297 sections 2.6 and 2.7). Follows the one-shot AEAD
 * contract above. key is TC_AES_SIV_KEYLEN bytes: the S2V CMAC key, then the
 * CTR key. Associated data is an array of 0..TC_AES_SIV_MAX_AD spans, read in
 * place (empty components are valid). ad may be NULL when ad_count is 0.
 * The ad array and every AD span must be disjoint from the text output,
 * because decrypt runs S2V over the AD after writing candidate plaintext. Ciphertext length
 * equals plaintext length. v is the 16-byte synthetic IV, written by encrypt
 * only on success and read by decrypt. v may alias plaintext when ciphertext
 * is distinct. Any overlap between v and the text output returns TC_ERROR.
 * Decrypt writes candidate plaintext, then verifies. A mismatch returns
 * TC_MISMATCH and wipes the output.
 */
TC_status TC_AES_SIV_encrypt(TC_bytes key, const TC_bytes* ad, size_t ad_count, TC_bytes plaintext,
                             TC_buffer v, TC_buffer ciphertext);
TC_status TC_AES_SIV_decrypt(TC_bytes key, const TC_bytes* ad, size_t ad_count, TC_bytes v,
                             TC_bytes ciphertext, TC_buffer plaintext);
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* TINY_CRYPTO_AES_H_ */
