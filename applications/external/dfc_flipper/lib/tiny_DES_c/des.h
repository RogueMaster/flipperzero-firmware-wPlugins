/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#ifndef TINY_DES_H_
#define TINY_DES_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @file des.h
 * @brief Portable C implementation of DES and Triple-DES (3DES / TDES).
 *
 * Designed for microcontrollers and embedded devices.
 * Inspired by kokke's tiny-AES-c.
 */

/* Status codes used by fallible APIs in this library. */
#define DES_OK   0
#define DES_ERR  (-1)

/* When 1 (default), one-shot paths wipe stack secrets on exit. */
#ifndef DES_ZEROIZE
  #define DES_ZEROIZE 1
#endif

#if (DES_ZEROIZE != 0) && (DES_ZEROIZE != 1)
  #error "DES_ZEROIZE must be 0 or 1"
#endif

/* When 1, classical buffer APIs reject NULL arguments (compiled out when 0). */
#ifndef DES_STRICT
  #define DES_STRICT 0
#endif

#if (DES_STRICT != 0) && (DES_STRICT != 1)
  #error "DES_STRICT must be 0 or 1"
#endif

/*
 * Mode selection (define to 1/0 before including this header, or via -D).
 *
 * Default build enables CTR and Triple-DES only. ECB, CBC, CFB*, OFB, and
 * CMAC are opt-in so unused modes do not contribute code or context fields.
 * Only DES_ENABLE_* names are used (no bare CBC/CTR macros) so this header
 * can co-exist with aes.h in the same translation unit.
 */
#ifndef DES_ENABLE_ECB
  #define DES_ENABLE_ECB 0
#endif
#ifndef DES_ENABLE_CBC
  #define DES_ENABLE_CBC 0
#endif
#ifndef DES_ENABLE_CTR
  #define DES_ENABLE_CTR 1
#endif
#ifndef DES_ENABLE_OFB
  #define DES_ENABLE_OFB 0
#endif
#ifndef DES_ENABLE_CFB1
  #define DES_ENABLE_CFB1 0
#endif
#ifndef DES_ENABLE_CFB8
  #define DES_ENABLE_CFB8 0
#endif
#ifndef DES_ENABLE_CFB64
  #define DES_ENABLE_CFB64 0
#endif
#ifndef DES_ENABLE_TDES
  #define DES_ENABLE_TDES 1
#endif
#ifndef DES_ENABLE_CMAC
  #define DES_ENABLE_CMAC 0
#endif

#if (DES_ENABLE_ECB != 0) && (DES_ENABLE_ECB != 1)
  #error "DES_ENABLE_ECB must be 0 or 1"
#endif
#if (DES_ENABLE_CBC != 0) && (DES_ENABLE_CBC != 1)
  #error "DES_ENABLE_CBC must be 0 or 1"
#endif
#if (DES_ENABLE_CTR != 0) && (DES_ENABLE_CTR != 1)
  #error "DES_ENABLE_CTR must be 0 or 1"
#endif
#if (DES_ENABLE_OFB != 0) && (DES_ENABLE_OFB != 1)
  #error "DES_ENABLE_OFB must be 0 or 1"
#endif
#if (DES_ENABLE_CFB1 != 0) && (DES_ENABLE_CFB1 != 1)
  #error "DES_ENABLE_CFB1 must be 0 or 1"
#endif
#if (DES_ENABLE_CFB8 != 0) && (DES_ENABLE_CFB8 != 1)
  #error "DES_ENABLE_CFB8 must be 0 or 1"
#endif
#if (DES_ENABLE_CFB64 != 0) && (DES_ENABLE_CFB64 != 1)
  #error "DES_ENABLE_CFB64 must be 0 or 1"
#endif
#if (DES_ENABLE_TDES != 0) && (DES_ENABLE_TDES != 1)
  #error "DES_ENABLE_TDES must be 0 or 1"
#endif
#if (DES_ENABLE_CMAC != 0) && (DES_ENABLE_CMAC != 1)
  #error "DES_ENABLE_CMAC must be 0 or 1"
#endif

/* Modes that keep chaining state in ctx->Iv */
#if (DES_ENABLE_CBC == 1) || (DES_ENABLE_CTR == 1) || (DES_ENABLE_CFB1 == 1) || \
    (DES_ENABLE_CFB8 == 1) || (DES_ENABLE_CFB64 == 1) || (DES_ENABLE_OFB == 1)
  #define DES_NEEDS_IV 1
#else
  #define DES_NEEDS_IV 0
#endif

#define DES_BLOCKLEN     8  /**< Block length in bytes - DES is a 64-bit (8 bytes) block cipher */
#define DES_KEYLEN       8  /**< Single DES key length in bytes (64 bits total, 56 bits effective) */

#define DES3_KEYLEN_2KEY 16 /**< 2-Key Triple DES key length in bytes (128 bits total, 112 bits effective) */
#define DES3_KEYLEN_3KEY 24 /**< 3-Key Triple DES key length in bytes (192 bits total, 168 bits effective) */

/**
 * @brief Single DES Context Structure
 */
struct DES_ctx
{
  uint32_t Sk[16][2];
#if DES_NEEDS_IV
  uint8_t Iv[DES_BLOCKLEN];
#endif
};

#if DES_ENABLE_TDES
/**
 * @brief Triple DES (3DES / TDES) Context Structure
 */
struct DES3_ctx
{
  uint32_t Sk[48][2];
#if DES_NEEDS_IV
  uint8_t Iv[DES_BLOCKLEN];
#endif
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Best-effort wipe of sensitive bytes (volatile stores; not a formal barrier). */
void DES_secure_zero(void* memory, size_t length);

/* Wipe a DES context (subkeys and IV when present). */
void DES_ctx_clear(struct DES_ctx* ctx);

#if DES_ENABLE_TDES
/* Wipe a Triple DES context (subkeys and IV when present). */
void DES3_ctx_clear(struct DES3_ctx* ctx);
#endif

/* --- Single DES API --- */

/**
 * @brief Initialize a Single DES context with key.
 * @param ctx Pointer to Single DES context structure.
 * @param key Pointer to 8-byte key buffer.
 */
void DES_init_ctx(struct DES_ctx* ctx, const uint8_t* key);

#if DES_NEEDS_IV
/**
 * @brief Initialize a Single DES context with key and IV.
 * @param ctx Pointer to Single DES context structure.
 * @param key Pointer to 8-byte key buffer.
 * @param iv Pointer to 8-byte Initialization Vector.
 */
void DES_init_ctx_iv(struct DES_ctx* ctx, const uint8_t* key, const uint8_t* iv);

/**
 * @brief Set or update the Initialization Vector (IV) in Single DES context.
 * @param ctx Pointer to Single DES context structure.
 * @param iv Pointer to 8-byte Initialization Vector.
 */
void DES_ctx_set_iv(struct DES_ctx* ctx, const uint8_t* iv);
#endif

#if DES_ENABLE_ECB
/**
 * @brief Encrypt an 8-byte block in ECB mode using Single DES.
 * @param ctx Pointer to initialized Single DES context.
 * @param buf Pointer to 8-byte data block (encrypted in-place).
 */
void DES_ECB_encrypt(const struct DES_ctx* ctx, uint8_t* buf);

/**
 * @brief Decrypt an 8-byte block in ECB mode using Single DES.
 * @param ctx Pointer to initialized Single DES context.
 * @param buf Pointer to 8-byte data block (decrypted in-place).
 */
void DES_ECB_decrypt(const struct DES_ctx* ctx, uint8_t* buf);
#endif

#if DES_ENABLE_CBC
/**
 * @brief Encrypt buffer in CBC mode using Single DES.
 * @param ctx Pointer to initialized Single DES context.
 * @param buf Data buffer (length must be a multiple of 8 bytes). Encrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 * @return DES_OK, or DES_ERR if length is not block-aligned.
 */
int DES_CBC_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in CBC mode using Single DES.
 * @param ctx Pointer to initialized Single DES context.
 * @param buf Data buffer (length must be a multiple of 8 bytes). Decrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 * @return DES_OK, or DES_ERR if length is not block-aligned.
 */
int DES_CBC_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CTR
/**
 * @brief Encrypt/Decrypt buffer in Counter (CTR) stream mode using Single DES.
 * @param ctx Pointer to initialized Single DES context.
 * @param buf Data buffer (arbitrary length). Transformed in-place.
 * @param length Data length in bytes.
 * @return DES_OK, or DES_ERR if the request would wrap the 64-bit counter
 *         (buffer and IV left unchanged).
 */
int DES_CTR_crypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB64
/**
 * @brief Encrypt buffer in 64-bit Cipher Feedback (CFB64) mode using Single DES.
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length; final segment may be shorter than 8).
 * @param length Data length in bytes.
 * @return DES_OK, or DES_ERR under DES_STRICT NULL checks.
 */
int DES_CFB64_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in 64-bit Cipher Feedback (CFB64) mode using Single DES.
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length; final segment may be shorter than 8).
 * @param length Data length in bytes.
 * @return DES_OK, or DES_ERR under DES_STRICT NULL checks.
 */
int DES_CFB64_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB8
/**
 * @brief Encrypt buffer in 8-bit Cipher Feedback (CFB8) mode using Single DES.
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length). Encrypted in-place.
 * @param length Data length in bytes.
 */
int DES_CFB8_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in 8-bit Cipher Feedback (CFB8) mode using Single DES.
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length). Decrypted in-place.
 * @param length Data length in bytes.
 */
int DES_CFB8_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB1
/**
 * @brief Encrypt bits in 1-bit Cipher Feedback (CFB1) mode using Single DES.
 *
 * Bits are packed MSB-first: bit i of the stream is (buf[i/8] >> (7 - i%8)) & 1.
 * Trailing pad bits of the final byte are left unchanged.
 *
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Packed bit buffer. Encrypted in-place.
 * @param bit_length Data length in bits.
 */
int DES_CFB1_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t bit_length);

/**
 * @brief Decrypt bits in 1-bit Cipher Feedback (CFB1) mode using Single DES.
 *
 * Bits are packed MSB-first: bit i of the stream is (buf[i/8] >> (7 - i%8)) & 1.
 * Trailing pad bits of the final byte are left unchanged.
 *
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Packed bit buffer. Decrypted in-place.
 * @param bit_length Data length in bits.
 */
int DES_CFB1_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t bit_length);
#endif

#if DES_ENABLE_OFB
/**
 * @brief Encrypt/Decrypt buffer in Output Feedback (OFB) stream mode using Single DES.
 * @param ctx Pointer to initialized Single DES context (IV holds chaining state).
 * @param buf Data buffer (length must be a multiple of 8 bytes). Transformed in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES_OFB_crypt(struct DES_ctx* ctx, uint8_t* buf, size_t length);
#endif


/* --- Triple DES (3DES / TDES) API --- */
#if DES_ENABLE_TDES

/**
 * @brief Initialize a Triple DES (3DES) context with key.
 * @param ctx Pointer to 3DES context structure.
 * @param key Pointer to key buffer (16 bytes for 2-Key 3DES, 24 bytes for 3-Key 3DES).
 * @param keylen Key length in bytes (16 or 24).
 * @return DES_OK on success, DES_ERR if keylen is not 16 or 24.
 */
int DES3_init_ctx(struct DES3_ctx* ctx, const uint8_t* key, size_t keylen);

#if DES_NEEDS_IV
/**
 * @brief Initialize a Triple DES (3DES) context with key and IV.
 * @param ctx Pointer to 3DES context structure.
 * @param key Pointer to key buffer (16 or 24 bytes).
 * @param keylen Key length in bytes (16 or 24).
 * @param iv Pointer to 8-byte Initialization Vector.
 * @return DES_OK on success, DES_ERR if keylen is not 16 or 24.
 */
int DES3_init_ctx_iv(struct DES3_ctx* ctx, const uint8_t* key, size_t keylen, const uint8_t* iv);

/**
 * @brief Set or update the Initialization Vector (IV) in 3DES context.
 * @param ctx Pointer to 3DES context structure.
 * @param iv Pointer to 8-byte Initialization Vector.
 */
void DES3_ctx_set_iv(struct DES3_ctx* ctx, const uint8_t* iv);
#endif

#if DES_ENABLE_ECB
/**
 * @brief Encrypt an 8-byte block in ECB mode using 3DES.
 * @param ctx Pointer to initialized 3DES context.
 * @param buf Pointer to 8-byte data block (encrypted in-place).
 */
void DES3_ECB_encrypt(const struct DES3_ctx* ctx, uint8_t* buf);

/**
 * @brief Decrypt an 8-byte block in ECB mode using 3DES.
 * @param ctx Pointer to initialized 3DES context.
 * @param buf Pointer to 8-byte data block (decrypted in-place).
 */
void DES3_ECB_decrypt(const struct DES3_ctx* ctx, uint8_t* buf);
#endif

#if DES_ENABLE_CBC
/**
 * @brief Encrypt buffer in CBC mode using 3DES.
 * @param ctx Pointer to initialized 3DES context.
 * @param buf Data buffer (length must be a multiple of 8 bytes). Encrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES3_CBC_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in CBC mode using 3DES.
 * @param ctx Pointer to initialized 3DES context.
 * @param buf Data buffer (length must be a multiple of 8 bytes). Decrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES3_CBC_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CTR
/**
 * @brief Encrypt/Decrypt buffer in Counter (CTR) stream mode using 3DES.
 * @param ctx Pointer to initialized 3DES context.
 * @param buf Data buffer (arbitrary length). Transformed in-place.
 * @param length Data length in bytes.
 */
int DES3_CTR_crypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB64
/**
 * @brief Encrypt buffer in 64-bit Cipher Feedback (CFB64) mode using 3DES.
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Data buffer (length must be a multiple of 8 bytes). Encrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES3_CFB64_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in 64-bit Cipher Feedback (CFB64) mode using 3DES.
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Data buffer (length must be a multiple of 8 bytes). Decrypted in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES3_CFB64_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB8
/**
 * @brief Encrypt buffer in 8-bit Cipher Feedback (CFB8) mode using 3DES.
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length). Encrypted in-place.
 * @param length Data length in bytes.
 */
int DES3_CFB8_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);

/**
 * @brief Decrypt buffer in 8-bit Cipher Feedback (CFB8) mode using 3DES.
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Data buffer (arbitrary length). Decrypted in-place.
 * @param length Data length in bytes.
 */
int DES3_CFB8_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if DES_ENABLE_CFB1
/**
 * @brief Encrypt bits in 1-bit Cipher Feedback (CFB1) mode using 3DES.
 *
 * Bits are packed MSB-first: bit i of the stream is (buf[i/8] >> (7 - i%8)) & 1.
 * Trailing pad bits of the final byte are left unchanged.
 *
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Packed bit buffer. Encrypted in-place.
 * @param bit_length Data length in bits.
 */
int DES3_CFB1_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t bit_length);

/**
 * @brief Decrypt bits in 1-bit Cipher Feedback (CFB1) mode using 3DES.
 *
 * Bits are packed MSB-first: bit i of the stream is (buf[i/8] >> (7 - i%8)) & 1.
 * Trailing pad bits of the final byte are left unchanged.
 *
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Packed bit buffer. Decrypted in-place.
 * @param bit_length Data length in bits.
 */
int DES3_CFB1_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t bit_length);
#endif

#if DES_ENABLE_OFB
/**
 * @brief Encrypt/Decrypt buffer in Output Feedback (OFB) stream mode using 3DES.
 * @param ctx Pointer to initialized 3DES context (IV holds chaining state).
 * @param buf Data buffer (length must be a multiple of 8 bytes). Transformed in-place.
 * @param length Data length in bytes (must be a multiple of 8).
 */
int DES3_OFB_crypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length);
#endif

#endif /* #if DES_ENABLE_TDES */


/* --- DES / 3DES CMAC (NIST SP 800-38B) --- */
#if DES_ENABLE_CMAC

/* Full CMAC tag is one DES block; shorter tags are the leading tag_len bytes. */
#define DES_CMAC_TAG_MAX DES_BLOCKLEN

/*
 * Minimum CMAC tag length in bytes. SP 800-38B recommends Tlen >= 64 bits for
 * most applications; shorter tags need careful risk analysis. Default 8 (full
 * DES block). Override only for exotic vectors.
 */
#ifndef DES_CMAC_MIN_TAG_LEN
  #define DES_CMAC_MIN_TAG_LEN 8
#endif

#if (DES_CMAC_MIN_TAG_LEN < 1) || (DES_CMAC_MIN_TAG_LEN > DES_BLOCKLEN)
  #error "DES_CMAC_MIN_TAG_LEN must be in 1..8"
#endif

/*
 * DES/3DES-CMAC (NIST SP 800-38B). Heap-free one-shot.
 * keylen must be 8 (single DES), 16 (2-key TDEA), or 24 (3-key TDEA).
 * tag_len must be in DES_CMAC_MIN_TAG_LEN..DES_CMAC_TAG_MAX.
 * Empty message: msg may be NULL when msg_len is 0.
 * Stack secrets wiped when DES_ZEROIZE=1.
 */
int DES_CMAC(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len);

/* Constant-time verify of a (possibly truncated) tag. */
int DES_CMAC_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len);

#endif /* DES_ENABLE_CMAC */

#ifdef __cplusplus
}
#endif

#endif /* TINY_DES_H_ */
