/*
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */
#ifndef TINY_AES_H_
#define TINY_AES_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes used by every fallible API in this library. */
#define AES_OK   0
#define AES_ERR  (-1)

/*
 * Mode selection (define to 1/0 before including this header, or via -D).
 *
 * Default build enables CTR only. CBC, ECB, OFB, CCM, EAX, EAX_PRIME, GCM,
 * SIV, and CMAC are opt-in so unused modes do not contribute code or context
 * fields.
 */
#ifndef AES_ENABLE_CBC
  #define AES_ENABLE_CBC 0
#endif

#ifndef AES_ENABLE_ECB
  #define AES_ENABLE_ECB 0
#endif

#ifndef AES_ENABLE_CTR
  #define AES_ENABLE_CTR 1
#endif

#ifndef AES_ENABLE_OFB
  #define AES_ENABLE_OFB 0
#endif

#ifndef AES_ENABLE_GCM
  #define AES_ENABLE_GCM 0
#endif

#ifndef AES_ENABLE_CCM
  #define AES_ENABLE_CCM 0
#endif

#ifndef AES_ENABLE_EAX
  #define AES_ENABLE_EAX 0
#endif

#ifndef AES_ENABLE_EAX_PRIME
  #define AES_ENABLE_EAX_PRIME 0
#endif

#ifndef AES_ENABLE_SIV
  #define AES_ENABLE_SIV 0
#endif

#ifndef AES_ENABLE_CMAC
  #define AES_ENABLE_CMAC 0
#endif

/* When 1 (default), one-shot paths wipe stack key material on exit. */
#ifndef AES_ZEROIZE
  #define AES_ZEROIZE 1
#endif

#if (AES_ZEROIZE != 0) && (AES_ZEROIZE != 1)
  #error "AES_ZEROIZE must be 0 or 1"
#endif

/* When 1, classical buffer APIs reject NULL arguments (compiled out when 0). */
#ifndef AES_STRICT
  #define AES_STRICT 0
#endif

#if (AES_STRICT != 0) && (AES_STRICT != 1)
  #error "AES_STRICT must be 0 or 1"
#endif

/* Minimum EAX tag length (not used by EAX'). Override only for exotic vectors. */
#ifndef AES_EAX_MIN_TAG_LEN
  #define AES_EAX_MIN_TAG_LEN 8
#endif

/*
 * Minimum CMAC tag length in bytes. SP 800-38B recommends Tlen >= 64 bits for
 * most applications; shorter tags need careful risk analysis. Default matches
 * AES_EAX_MIN_TAG_LEN. Override only for exotic vectors / CAVP short-tag rows.
 */
#ifndef AES_CMAC_MIN_TAG_LEN
  #define AES_CMAC_MIN_TAG_LEN 8
#endif

#if (AES_CMAC_MIN_TAG_LEN < 1) || (AES_CMAC_MIN_TAG_LEN > 16)
  #error "AES_CMAC_MIN_TAG_LEN must be in 1..16"
#endif

/*
 * AES_TINY=1 rejects GHASH table profiles (~8.5 KiB per context). Prefer
 * bitwise/auto/wide GHASH on small MCUs. Table modes always use a per-context
 * table (a shared table cannot safely serve multiple keys).
 */
#ifndef AES_TINY
  #define AES_TINY 0
#endif

#if (AES_TINY != 0) && (AES_TINY != 1)
  #error "AES_TINY must be 0 or 1"
#endif

/* GCM GHASH implementation profiles. */
#define AES_GCM_GHASH_MODE_AUTO       0
#define AES_GCM_GHASH_MODE_BITWISE    1
#define AES_GCM_GHASH_MODE_WIDE       2
#define AES_GCM_GHASH_MODE_TABLE4     3
#define AES_GCM_GHASH_MODE_FAST_TABLE 4
#define AES_GCM_GHASH_MODE_HARDWARE   5

#ifndef AES_GCM_GHASH_MODE
  #define AES_GCM_GHASH_MODE AES_GCM_GHASH_MODE_AUTO
#endif

#if (AES_GCM_GHASH_MODE < AES_GCM_GHASH_MODE_AUTO) || \
    (AES_GCM_GHASH_MODE > AES_GCM_GHASH_MODE_HARDWARE)
  #error "AES_GCM_GHASH_MODE is invalid"
#endif

#if (AES_TINY == 1) && \
    ((AES_GCM_GHASH_MODE == AES_GCM_GHASH_MODE_TABLE4) || \
     (AES_GCM_GHASH_MODE == AES_GCM_GHASH_MODE_FAST_TABLE))
  #error "AES_TINY forbids table4/fast-table GHASH (large AES_GCM_ctx)"
#endif

#if defined(AES_GCM_GHASH_HARDWARE_MULTIPLY)
void AES_GCM_GHASH_HARDWARE_MULTIPLY(uint8_t* result,
                                     const uint8_t* left,
                                     const uint8_t* right);
#endif

/*
 * S-box implementation modes:
 *   AES_SBOX_MODE_CONSTANT_TIME - fixed-size masked scan (default)
 *   AES_SBOX_MODE_RUNTIME       - generated in RAM, then masked scan
 *   AES_SBOX_MODE_FAST          - direct lookup; not constant-time
 */
#define AES_SBOX_MODE_CONSTANT_TIME 1
#define AES_SBOX_MODE_RUNTIME       2
#define AES_SBOX_MODE_FAST          3

#ifndef AES_SBOX_MODE
  #define AES_SBOX_MODE AES_SBOX_MODE_CONSTANT_TIME
#endif

#if (AES_SBOX_MODE < AES_SBOX_MODE_CONSTANT_TIME) || \
    (AES_SBOX_MODE > AES_SBOX_MODE_FAST)
  #error "AES_SBOX_MODE must be AES_SBOX_MODE_CONSTANT_TIME, AES_SBOX_MODE_RUNTIME, or AES_SBOX_MODE_FAST"
#endif

/* 0 keeps byte-safe operations; 1 enables portable native-width helpers. */
#ifndef AES_WIDE_OPS
  #define AES_WIDE_OPS 0
#endif

#if (AES_WIDE_OPS != 0) && (AES_WIDE_OPS != 1)
  #error "AES_WIDE_OPS must be 0 or 1"
#endif

/* AES128 is the default when no key-size macro is supplied by the build. */
#if !defined(AES128) && !defined(AES192) && !defined(AES256)
  #define AES128 1
#endif

#define AES_BLOCKLEN 16 /* AES block length in bytes (128-bit block only). */

#if defined(AES256) && (AES256 == 1)
    #define AES_KEYLEN 32
    #define AES_KEY_EXP_SIZE 240
#elif defined(AES192) && (AES192 == 1)
    #define AES_KEYLEN 24
    #define AES_KEY_EXP_SIZE 208
#else
    #define AES_KEYLEN 16
    #define AES_KEY_EXP_SIZE 176
#endif

struct AES_ctx
{
  uint8_t RoundKey[AES_KEY_EXP_SIZE];
#if (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || (defined(AES_ENABLE_CTR) && (AES_ENABLE_CTR == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1))
  uint8_t Iv[AES_BLOCKLEN];
#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
  uint8_t ofb_pos;
#endif
#endif
};

/* Best-effort wipe of sensitive bytes (volatile stores; not a formal barrier). */
void AES_secure_zero(void* memory, size_t length);

/* Wipe an AES context (round keys, IV, and OFB position when present). */
void AES_ctx_clear(struct AES_ctx* ctx);

void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key);
#if defined(AES_CAVP) && (AES_CAVP == 1)
/* Test-only forward-cipher hook used by the AESAVS Monte Carlo harness. */
void AES_CAVP_encrypt_block(const uint8_t* key, uint8_t block[AES_BLOCKLEN]);
void AES_CAVP_decrypt_block(const uint8_t* key, uint8_t block[AES_BLOCKLEN]);
#endif
#if AES_SBOX_MODE == AES_SBOX_MODE_RUNTIME
/* Must be called once before AES_init_ctx(), AES_init_ctx_iv(), or encryption. */
void AES_init_sbox(void);
#endif
#if (defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)) || (defined(AES_ENABLE_CTR) && (AES_ENABLE_CTR == 1)) || \
    (defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1))
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
void AES_ctx_set_iv(struct AES_ctx* ctx, const uint8_t* iv);
#endif

#if defined(AES_ENABLE_ECB) && (AES_ENABLE_ECB == 1)
/* Buffer must be exactly AES_BLOCKLEN bytes. ECB is insecure for most uses. */
void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf);
#endif

#if defined(AES_ENABLE_CBC) && (AES_ENABLE_CBC == 1)
/*
 * Buffer length must be a multiple of AES_BLOCKLEN (no padding is applied).
 * Returns AES_ERR if length is not block-aligned. Set IV via AES_init_ctx_iv()
 * or AES_ctx_set_iv(). Never reuse an IV with the same key.
 */
int AES_CBC_encrypt(struct AES_ctx* ctx, uint8_t* buf, size_t length);
int AES_CBC_decrypt(struct AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(AES_ENABLE_CTR) && (AES_ENABLE_CTR == 1)
/*
 * Encrypt and decrypt are the same operation. The IV is incremented for every
 * block. Returns AES_ERR if the request would wrap the 128-bit counter (buf
 * and IV are left unchanged). Never reuse an IV with the same key.
 */
int AES_CTR_crypt(struct AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(AES_ENABLE_OFB) && (AES_ENABLE_OFB == 1)
/*
 * Encrypt and decrypt are the same operation. Never reuse an IV with the same
 * key. OFB provides confidentiality only.
 */
int AES_OFB_crypt(struct AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(AES_ENABLE_GCM) && (AES_ENABLE_GCM == 1)

/*
 * NIST SP 800-38D length limits (bit lengths converted to bytes):
 *   1 <= len(IV) <= 2^64-1 bits
 *   len(P) <= 2^39-256 bits  =>  AES_GCM_MAX_PLAINTEXT_BYTES
 *   len(A) <= 2^64-1 bits
 * Tag length t (bits) is one of 128,120,112,104,96,64,32 and is fixed for the
 * key for the life of this context (passed at init).
 *
 * Appendix C short-tag packet limits (most permissive table row):
 *   t=32: |C|+|A| <= 2^10 bytes per packet
 *   t=64: |C|+|A| <= 2^25 bytes per packet
 * Key lifetime / max decryption invocations remain the application's duty.
 */
#define AES_GCM_MAX_PLAINTEXT_BYTES  ((((uint64_t)1) << 36) - 32u)
#define AES_GCM_MAX_AAD_BYTES        (UINT64_MAX / 8u)
#define AES_GCM_MAX_IV_BYTES         (UINT64_MAX / 8u)
#define AES_GCM_SHORT_TAG4_MAX_PACKET  ((uint64_t)1 << 10)  /* 1024 */
#define AES_GCM_SHORT_TAG8_MAX_PACKET  ((uint64_t)1 << 25)  /* 33554432 */

struct AES_GCM_ctx
{
  struct AES_ctx aes;
  uint8_t H[AES_BLOCKLEN];
  uint8_t J0[AES_BLOCKLEN];
  uint8_t counter[AES_BLOCKLEN];
  uint8_t stream[AES_BLOCKLEN];
  uint8_t S[AES_BLOCKLEN];
  uint8_t ghash[AES_BLOCKLEN];
#if (AES_GCM_GHASH_MODE == AES_GCM_GHASH_MODE_TABLE4) || \
    (AES_GCM_GHASH_MODE == AES_GCM_GHASH_MODE_FAST_TABLE)
  /* ~8 KiB per context; avoid with AES_TINY or use bitwise/auto GHASH. */
  uint8_t ghash_table[32][16][AES_BLOCKLEN];
#endif

  uint64_t aad_len;
  uint64_t text_len;
  size_t stream_pos;
  size_t ghash_len;
  uint8_t tag_len; /* fixed for this key/context; SP 800-38D §5.2.1.2 */
  uint8_t phase;
  uint8_t direction;
};

/*
 * Initialize GCM. tag_len is the SP 800-38D tag length t in bytes
 * (4, 8, or 12–16) and is fixed for this context. IV may be any supported
 * non-zero byte length; 12 bytes (96 bits) is the recommended fast path.
 */
int AES_GCM_init(struct AES_GCM_ctx* ctx, const uint8_t* key,
                 const uint8_t* iv, size_t iv_len, size_t tag_len);

/* AAD must be supplied before the first encrypt/decrypt update. A context is
 * single-direction; reinitialize before switching direction. Check every
 * return value. */
int AES_GCM_aad_update(struct AES_GCM_ctx* ctx, const uint8_t* aad,
                       size_t length);
int AES_GCM_encrypt_update(struct AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length);
int AES_GCM_decrypt_update(struct AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length);

/* Tag buffer must hold ctx->tag_len bytes (set at init). */
int AES_GCM_encrypt_finish(struct AES_GCM_ctx* ctx, uint8_t* tag);
int AES_GCM_decrypt_finish(struct AES_GCM_ctx* ctx, const uint8_t* tag);

/*
 * One-shot GCM. tag_len is fixed for this key use (SP 800-38D).
 * Buffer contract (all one-shot AEAD): exact alias of in/out is OK; fully
 * disjoint is OK; partial overlap returns AES_ERR.
 * Decrypt authenticates before releasing plaintext; on failure a non-aliasing
 * output is left untouched and an in-place buffer is zeroed.
 */
int AES_GCM_encrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
int AES_GCM_decrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len,
                    uint8_t* plaintext);

/* Clear expanded key material and intermediate authentication state. */
void AES_GCM_clear(struct AES_GCM_ctx* ctx);

#endif /* AES_ENABLE_GCM */

#if defined(AES_ENABLE_CCM) && (AES_ENABLE_CCM == 1)

/* CCM is a packet mode: payload and AAD lengths are known at entry. */
int AES_CCM_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
int AES_CCM_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len,
                    uint8_t* plaintext);

#endif

#if defined(AES_ENABLE_EAX) && (AES_ENABLE_EAX == 1)

/* EAX one-shot AEAD. Tags must be AES_EAX_MIN_TAG_LEN..16. Auth failure
 * leaves plaintext untouched. */
int AES_EAX_encrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len);
int AES_EAX_decrypt(const uint8_t* key, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len, uint8_t* plaintext);

#endif

#if defined(AES_ENABLE_EAX_PRIME) && (AES_ENABLE_EAX_PRIME == 1)

#define AES_EAX_PRIME_TAG_LEN 4

/* ANSI C12.22 EAX'. Fixed four-byte tag. Auth failure leaves output untouched. */
int AES_EAX_PRIME_encrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* plaintext,
                          size_t plaintext_len, uint8_t* ciphertext,
                          uint8_t tag[AES_EAX_PRIME_TAG_LEN]);
int AES_EAX_PRIME_decrypt(const uint8_t* key, const uint8_t* cleartext,
                          size_t cleartext_len, const uint8_t* ciphertext,
                          size_t ciphertext_len,
                          const uint8_t tag[AES_EAX_PRIME_TAG_LEN],
                          uint8_t* plaintext);

#endif

#if defined(AES_ENABLE_CMAC) && (AES_ENABLE_CMAC == 1)

/* Full CMAC tag is one AES block; shorter tags are the leading tag_len bytes. */
#define AES_CMAC_TAG_MAX AES_BLOCKLEN

/*
 * AES-CMAC (NIST SP 800-38B). Heap-free one-shot.
 * tag_len must be in AES_CMAC_MIN_TAG_LEN..AES_CMAC_TAG_MAX (default min 8;
 * SP 800-38B truncation: most significant octets of the full T). Empty
 * message: msg may be NULL when msg_len is 0. Stack secrets wiped when
 * AES_ZEROIZE=1.
 */
int AES_CMAC(const uint8_t* key, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len);

/* Constant-time verify of a (possibly truncated) tag. */
int AES_CMAC_verify(const uint8_t* key, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len);

#endif

#if defined(AES_ENABLE_SIV) && (AES_ENABLE_SIV == 1)

/* RFC 5297 SIV-AES: key is two equal AES keys concatenated (CMAC || CTR). */
#define AES_SIV_KEYLEN   (AES_KEYLEN * 2)
#define AES_SIV_V_LEN    AES_BLOCKLEN
/* RFC §7: at most 126 associated-data components (plaintext is the last S2V input). */
#define AES_SIV_MAX_AD   126u

/*
 * One-shot SIV (RFC 5297). Associated data is a vector of 0..AES_SIV_MAX_AD
 * components (empty components are valid). Ciphertext length equals
 * plaintext length. pt/ct must be exact aliases or fully disjoint (partial
 * overlap returns AES_ERR). v may alias plaintext when ciphertext is
 * distinct, but must be disjoint from ciphertext (exact or partial overlap
 * with ct returns AES_ERR). Decrypt writes candidate plaintext then
 * verifies; on authentication failure the output is wiped.
 */
int AES_SIV_encrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t v[AES_SIV_V_LEN],
                    uint8_t* ciphertext);
int AES_SIV_decrypt(const uint8_t* key,
                    const uint8_t* const* ad, const size_t* ad_lens,
                    size_t ad_count,
                    const uint8_t v[AES_SIV_V_LEN],
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    uint8_t* plaintext);

#endif

#ifdef __cplusplus
}
#endif

#endif /* TINY_AES_H_ */
