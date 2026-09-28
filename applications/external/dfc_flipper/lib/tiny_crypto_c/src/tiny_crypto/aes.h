/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_AES_H_
#define TINY_CRYPTO_AES_H_

#include <tiny_crypto/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mode selection (define to 1/0 before including this header, or via -D).
 * Default build enables CTR only. CBC, ECB, OFB, CCM, EAX, EAX_PRIME, GCM,
 * SIV, and CMAC are opt-in so unused modes do not contribute code or context
 * fields.
 */

/*
 * Minimum CMAC tag length in bytes. SP 800-38B recommends Tlen >= 64 bits for
 * most applications; shorter tags need careful risk analysis. Default matches
 * TC_AES_EAX_MIN_TAG_LEN. Override only for exotic vectors / CAVP short-tag rows.
 */

#if (TC_AES_CMAC_MIN_TAG_LEN < 1) || (TC_AES_CMAC_MIN_TAG_LEN > 16)
  #error "TC_AES_CMAC_MIN_TAG_LEN must be in 1..16"
#endif

/*
 * TC_AES_TINY=1 rejects the 256-byte fast GHASH table. Prefer bitwise/auto/wide
 * GHASH on small MCUs. The table is per-context because it depends on the key.
 */

#if (TC_AES_TINY != 0) && (TC_AES_TINY != 1)
  #error "TC_AES_TINY must be 0 or 1"
#endif

/* GCM GHASH implementation profiles. */
#define TC_AES_GCM_GHASH_MODE_AUTO       0
#define TC_AES_GCM_GHASH_MODE_BITWISE    1
#define TC_AES_GCM_GHASH_MODE_WIDE       2
#define TC_AES_GCM_GHASH_MODE_FAST_TABLE 3
#define TC_AES_GCM_GHASH_MODE_HARDWARE   4

#if (TC_AES_GCM_GHASH_MODE < TC_AES_GCM_GHASH_MODE_AUTO) || \
    (TC_AES_GCM_GHASH_MODE > TC_AES_GCM_GHASH_MODE_HARDWARE)
  #error "TC_AES_GCM_GHASH_MODE is invalid"
#endif

#if (TC_AES_TINY == 1) && \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE)
  #error "TC_AES_TINY forbids the 256-byte fast GHASH table"
#endif

#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_HARDWARE
/* Platform hook required by the hardware GHASH profile. */
void TC_AES_GCM_hardware_multiply(uint8_t result[16],
                                  const uint8_t left[16],
                                  const uint8_t right[16]);
#endif

/*
 * S-box implementation modes:
 *   TC_AES_SBOX_MODE_CONSTANT_TIME - algebraic inversion (default)
 *   TC_AES_SBOX_MODE_RUNTIME       - generated in RAM, then masked scan
 *   TC_AES_SBOX_MODE_FAST          - direct lookup; not constant-time
 */
#define TC_AES_SBOX_MODE_CONSTANT_TIME 1
#define TC_AES_SBOX_MODE_RUNTIME       2
#define TC_AES_SBOX_MODE_FAST          3

#if (TC_AES_SBOX_MODE < TC_AES_SBOX_MODE_CONSTANT_TIME) || \
    (TC_AES_SBOX_MODE > TC_AES_SBOX_MODE_FAST)
  #error "TC_AES_SBOX_MODE must be TC_AES_SBOX_MODE_CONSTANT_TIME, TC_AES_SBOX_MODE_RUNTIME, or TC_AES_SBOX_MODE_FAST"
#endif

/* 0 keeps byte-safe operations; 1 enables portable native-width helpers. */
#if (TC_AES_WIDE_OPS != 0) && (TC_AES_WIDE_OPS != 1)
  #error "TC_AES_WIDE_OPS must be 0 or 1"
#endif

/* Compile exactly one AES key schedule size into a library profile. */
#if (TC_AES_KEY_BITS != 128) && (TC_AES_KEY_BITS != 192) && \
    (TC_AES_KEY_BITS != 256)
  #error "TC_AES_KEY_BITS must be 128, 192, or 256"
#endif

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

struct TC_AES_key_ctx
{
  uint8_t round_key[TC_AES_KEY_EXP_SIZE];
};

struct TC_AES_ctx
{
  struct TC_AES_key_ctx key;
#if (defined(TC_AES_ENABLE_CBC) && (TC_AES_ENABLE_CBC == 1)) || (defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)) || \
    (defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1))
  uint8_t iv[TC_AES_BLOCKLEN];
#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)
  uint8_t ctr_stream[TC_AES_BLOCKLEN];
  uint8_t ctr_pos;
#endif
#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)
  uint8_t ofb_pos;
#endif
#endif
};

TC_status TC_AES_key_init(struct TC_AES_key_ctx* ctx, const uint8_t* key);
void TC_AES_key_ctx_clear(struct TC_AES_key_ctx* ctx);

/* Wipe an AES context, including mode-specific streaming state. */
void TC_AES_ctx_clear(struct TC_AES_ctx* ctx);

/* Initialize an expanded key schedule. Both pointers must be non-NULL. */
TC_status TC_AES_init_ctx(struct TC_AES_ctx* ctx, const uint8_t* key);
#if defined(TC_AES_CAVP) && (TC_AES_CAVP == 1)
/* Test-only forward-cipher hook used by the AESAVS Monte Carlo harness. */
void TC_AES_CAVP_encrypt_block(const uint8_t* key, uint8_t block[TC_AES_BLOCKLEN]);
void TC_AES_CAVP_decrypt_block(const uint8_t* key, uint8_t block[TC_AES_BLOCKLEN]);
#endif
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
/* Must be called once before TC_AES_init_ctx(), TC_AES_init_ctx_iv(), or encryption. */
void TC_AES_init_sbox(void);
#endif
#if (defined(TC_AES_ENABLE_CBC) && (TC_AES_ENABLE_CBC == 1)) || (defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)) || \
    (defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1))
TC_status TC_AES_init_ctx_iv(struct TC_AES_ctx* ctx, const uint8_t* key,
                            const uint8_t* iv);
TC_status TC_AES_ctx_set_iv(struct TC_AES_ctx* ctx, const uint8_t* iv);
#endif

#if defined(TC_AES_ENABLE_ECB) && (TC_AES_ENABLE_ECB == 1)
/* Buffer must be exactly TC_AES_BLOCKLEN bytes. ECB is insecure for most uses. */
TC_status TC_AES_ECB_encrypt(const struct TC_AES_key_ctx* ctx, uint8_t* buf);
TC_status TC_AES_ECB_decrypt(const struct TC_AES_key_ctx* ctx, uint8_t* buf);
#endif

#if defined(TC_AES_ENABLE_CBC) && (TC_AES_ENABLE_CBC == 1)
/*
 * Buffer length must be a multiple of TC_AES_BLOCKLEN (no padding is applied).
 * Returns TC_ERROR if length is not block-aligned. Set IV via TC_AES_init_ctx_iv()
 * or TC_AES_ctx_set_iv(). Never reuse an IV with the same key.
 */
TC_status TC_AES_CBC_encrypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length);
TC_status TC_AES_CBC_decrypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)
/*
 * Encrypt and decrypt are the same operation. The IV is incremented for every
 * block. Returns TC_ERROR if the request would wrap the 128-bit counter (buf
 * and IV are left unchanged). Never reuse an IV with the same key.
 */
TC_status TC_AES_CTR_crypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)
/*
 * Encrypt and decrypt are the same operation. Never reuse an IV with the same
 * key. OFB provides confidentiality only.
 */
TC_status TC_AES_OFB_crypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(TC_AES_ENABLE_GCM) && (TC_AES_ENABLE_GCM == 1)

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
#define TC_AES_GCM_MAX_PLAINTEXT_BYTES  ((((uint64_t)1) << 36) - 32u)
#define TC_AES_GCM_MAX_AAD_BYTES        (UINT64_MAX / 8u)
#define TC_AES_GCM_MAX_IV_BYTES         (UINT64_MAX / 8u)
#define TC_AES_GCM_SHORT_TAG4_MAX_PACKET  ((uint64_t)1 << 10)  /* 1024 */
#define TC_AES_GCM_SHORT_TAG8_MAX_PACKET  ((uint64_t)1 << 25)  /* 33554432 */

struct TC_AES_GCM_ctx
{
  struct TC_AES_key_ctx key;
  uint8_t H[TC_AES_BLOCKLEN];
  uint8_t J0[TC_AES_BLOCKLEN];
  uint8_t counter[TC_AES_BLOCKLEN];
  uint8_t stream[TC_AES_BLOCKLEN];
  uint8_t S[TC_AES_BLOCKLEN];
  uint8_t ghash[TC_AES_BLOCKLEN];
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  uint8_t ghash_table[16][TC_AES_BLOCKLEN];
#endif

  uint64_t aad_len;
  uint64_t text_len;
  uint8_t stream_pos;
  uint8_t ghash_len;
  uint8_t tag_len; /* fixed for this key/context; SP 800-38D §5.2.1.2 */
  uint8_t phase;
  uint8_t direction;
};

/*
 * Initialize GCM. tag_len is the SP 800-38D tag length t in bytes
 * (4, 8, or 12–16) and is fixed for this context. IV may be any supported
 * non-zero byte length; 12 bytes (96 bits) is the recommended fast path.
 */
TC_status TC_AES_GCM_init(struct TC_AES_GCM_ctx* ctx, const uint8_t* key,
                 const uint8_t* iv, size_t iv_len, size_t tag_len);

/* AAD must be supplied before the first encrypt/decrypt update. A context is
 * single-direction; reinitialize before switching direction. Check every
 * return value. Streaming decryption writes provisional plaintext during
 * update; do not use it until decrypt_finish returns TC_OK. On any failure,
 * discard and wipe plaintext from all preceding updates. */
TC_status TC_AES_GCM_aad_update(struct TC_AES_GCM_ctx* ctx, const uint8_t* aad,
                       size_t length);
TC_status TC_AES_GCM_encrypt_update(struct TC_AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length);
TC_status TC_AES_GCM_decrypt_update(struct TC_AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length);

/* Tag buffer must hold ctx->tag_len bytes (set at init). */
TC_status TC_AES_GCM_encrypt_finish(struct TC_AES_GCM_ctx* ctx, uint8_t* tag);
TC_status TC_AES_GCM_decrypt_finish(struct TC_AES_GCM_ctx* ctx, const uint8_t* tag);

/*
 * One-shot GCM. tag_len is fixed for this key use (SP 800-38D).
 * Buffer contract (all one-shot AEAD): exact alias of in/out is OK; fully
 * disjoint is OK; partial overlap returns TC_ERROR.
 * Decrypt authenticates before releasing plaintext. GCM and CCM leave a
 * separate output untouched and wipe in-place ciphertext on a tag mismatch.
 * EAX leaves both kinds of output untouched on authentication failure.
 */
TC_status TC_AES_GCM_encrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
TC_status TC_AES_GCM_decrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len,
                    uint8_t* plaintext);

/* Clear expanded key material and intermediate authentication state. */
void TC_AES_GCM_clear(struct TC_AES_GCM_ctx* ctx);

#endif /* TC_AES_ENABLE_GCM */

#if defined(TC_AES_ENABLE_CCM) && (TC_AES_ENABLE_CCM == 1)

/* CCM is a packet mode: payload and AAD lengths are known at entry. */
TC_status TC_AES_CCM_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
TC_status TC_AES_CCM_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len,
                    uint8_t* plaintext);

#endif

#if defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)

/* EAX one-shot AEAD. Tags must be TC_AES_EAX_MIN_TAG_LEN..16. Auth failure
 * leaves plaintext untouched. */
TC_status TC_AES_EAX_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
TC_status TC_AES_EAX_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len, uint8_t* plaintext);

#endif

#if defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1)

#define TC_AES_EAX_PRIME_TAG_LEN 4

/* ANSI C12.22 EAX'. Fixed four-byte tag. Auth failure leaves output untouched. */
TC_status TC_AES_EAX_PRIME_encrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* plaintext,
                          size_t plaintext_len, uint8_t* ciphertext,
                          uint8_t tag[TC_AES_EAX_PRIME_TAG_LEN]);
TC_status TC_AES_EAX_PRIME_decrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* ciphertext,
                          size_t ciphertext_len,
                          const uint8_t tag[TC_AES_EAX_PRIME_TAG_LEN],
                          uint8_t* plaintext);

#endif

#if defined(TC_AES_ENABLE_CMAC) && (TC_AES_ENABLE_CMAC == 1)

/* Full CMAC tag is one AES block; shorter tags are the leading tag_len bytes. */
#define TC_AES_CMAC_TAG_MAX TC_AES_BLOCKLEN

/*
 * AES-CMAC (NIST SP 800-38B). Heap-free one-shot.
 * tag_len must be in TC_AES_CMAC_MIN_TAG_LEN..TC_AES_CMAC_TAG_MAX (default min 8;
 * SP 800-38B truncation: most significant octets of the full T). Empty
 * message: msg may be NULL when msg_len is 0. Stack secrets wiped when
 * TC_ZEROIZE=1.
 */
TC_status TC_AES_CMAC(const uint8_t* key, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len);

/* Constant-time verify of a (possibly truncated) tag. */
TC_status TC_AES_CMAC_verify(const uint8_t* key, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len);

/*
 * Streaming AES-CMAC. The most recent block is held back in buf so that
 * *_final can apply K1 (complete) or K2 (padded) to the true last block.
 * *_final always emits the full TC_AES_CMAC_TAG_MAX bytes; truncate at the
 * call site if required. The context is consumed by *_final and wiped when
 * TC_ZEROIZE is 1; call *_init again before reuse.
 */
struct TC_AES_CMAC_ctx
{
  struct TC_AES_key_ctx key;
  uint8_t k1[TC_AES_BLOCKLEN];
  uint8_t k2[TC_AES_BLOCKLEN];
  uint8_t mac[TC_AES_BLOCKLEN];
  uint8_t buf[TC_AES_BLOCKLEN];
  uint8_t buf_len;
};

TC_status TC_AES_CMAC_init(struct TC_AES_CMAC_ctx* ctx, const uint8_t* key);
TC_status TC_AES_CMAC_update(struct TC_AES_CMAC_ctx* ctx, const uint8_t* data,
                             size_t len);
TC_status TC_AES_CMAC_final(struct TC_AES_CMAC_ctx* ctx,
                            uint8_t tag[TC_AES_CMAC_TAG_MAX]);
void TC_AES_CMAC_ctx_clear(struct TC_AES_CMAC_ctx* ctx);

#endif

#if defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1)

/* RFC 5297 SIV-AES: key is two equal AES keys concatenated (CMAC || CTR). */
#define TC_AES_SIV_KEYLEN   (TC_AES_KEYLEN * 2)
#define TC_AES_SIV_V_LEN    TC_AES_BLOCKLEN
/* RFC §7: at most 126 associated-data components (plaintext is the last S2V input). */
#define TC_AES_SIV_MAX_AD   126u

/*
 * One-shot SIV (RFC 5297). Associated data is a vector of 0..TC_AES_SIV_MAX_AD
 * components (empty components are valid). Ciphertext length equals
 * plaintext length. pt/ct must be exact aliases or fully disjoint (partial
 * overlap returns TC_ERROR). v may alias plaintext when ciphertext is
 * distinct, but must be disjoint from ciphertext (exact or partial overlap
 * with ct returns TC_ERROR). Decrypt writes candidate plaintext then
 * verifies; on authentication failure the output is wiped.
 */
TC_status TC_AES_SIV_encrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t v[TC_AES_SIV_V_LEN],
                    uint8_t* ciphertext);
TC_status TC_AES_SIV_decrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t v[TC_AES_SIV_V_LEN],
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    uint8_t* plaintext);

#endif

#ifdef __cplusplus
}
#endif

#endif /* TINY_CRYPTO_AES_H_ */
