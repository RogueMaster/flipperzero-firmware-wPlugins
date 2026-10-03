/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * NIST SP 800-90A Rev. 1 deterministic random bit generators: Hash_DRBG,
 * HMAC_DRBG and CTR_DRBG with AES and an optional derivation function.
 *
 * A DRBG stretches seed material from a caller-supplied entropy source into
 * pseudorandom output. The library supplies the SP 800-90A mechanisms and the
 * section 9 instantiate, reseed, generate and uninstantiate functions. The
 * caller supplies an entropy source that meets SP 800-90B or an equivalent
 * requirement for the configured security strength. The DRBG is only as
 * strong as that source.
 *
 * All storage is caller-owned. A TC_DRBG holds the working state and the
 * scratch space its operations need, so calls use little stack.
 *
 * Configuration: TC_ENABLE_DRBG and TC_DRBG_ENABLE_HASH, _HMAC and _CTR.
 * Work: every function charges no work budget. Entropy requests are bounded
 * by TC_DRBG_MAX_ENTROPY_BYTES per instantiate or reseed.
 * Contracts: docs/api.md. Guide: docs/drbg.md. */
#ifndef TINY_CRYPTO_DRBG_H_
#define TINY_CRYPTO_DRBG_H_

#include <tiny_crypto/common.h>

#define TC_DRBG_HAVE_HASH (TC_ENABLE_DRBG && TC_DRBG_ENABLE_HASH)
#define TC_DRBG_HAVE_HMAC (TC_ENABLE_DRBG && TC_DRBG_ENABLE_HMAC)
#define TC_DRBG_HAVE_CTR (TC_ENABLE_DRBG && TC_DRBG_ENABLE_CTR)

#if TC_ENABLE_DRBG

#if TC_DRBG_HAVE_HASH || TC_DRBG_HAVE_HMAC
#include <tiny_crypto/hash.h>
#endif
#if TC_DRBG_HAVE_CTR
#include <tiny_crypto/aes_dynamic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SP 800-90A Table 2 and Table 3 limits shared by every mechanism. */
#define TC_DRBG_MAX_REQUEST_BYTES 65536u /* 2^19 bits per generate call */
#define TC_DRBG_MAX_RESEED_INTERVAL ((uint64_t)1 << 48)

/* TC_DRBG_MAX_ENTROPY_BYTES (config.h) is the largest entropy input accepted
 * per instantiate or reseed, including a nonce drawn from the entropy source.
 * It sizes TC_DRBG.input. */

/* Largest personalization string, nonce or additional input. SP 800-90A
 * Tables 2 and 3 allow 2^35 bits. This bound also leaves room for the entropy
 * input within the 32-bit length field of the CTR_DRBG derivation function
 * (section 10.3.2). Instantiate also bounds nonce plus personalization.
 * Larger inputs return TC_DRBG_ARGUMENT before any entropy is drawn. */
#define TC_DRBG_MAX_INPUT_BYTES (0xffffffffu - TC_DRBG_MAX_ENTROPY_BYTES)

/* Hash_DRBG seed length: 440 bits for SHA-1 and SHA-224/256, 888 bits for
 * SHA-384/512 (SP 800-90A Table 2). */
#if TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#define TC_DRBG_HASH_SEED_BYTES 111u
#else
#define TC_DRBG_HASH_SEED_BYTES 55u
#endif
#if TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#define TC_DRBG_HMAC_OUTPUT_BYTES 64u
#else
#define TC_DRBG_HMAC_OUTPUT_BYTES 32u
#endif

typedef enum {
  TC_DRBG_HASH = 1, /* Hash_DRBG, SP 800-90A section 10.1.1 */
  TC_DRBG_HMAC = 2, /* HMAC_DRBG, section 10.1.2 */
  TC_DRBG_CTR = 3   /* CTR_DRBG with AES, section 10.2.1 */
} TC_DRBG_mechanism;

/* A DRBG that returned TC_DRBG_ERROR stays unusable. Later reseed and
 * generate calls return TC_DRBG_ARGUMENT until it is instantiated again. */
typedef TC_result TC_DRBG_result;
#define TC_DRBG_OK TC_RESULT_OK
#define TC_DRBG_ARGUMENT TC_RESULT_ARGUMENT       /* invalid argument or state */
#define TC_DRBG_UNSUPPORTED TC_RESULT_UNSUPPORTED /* mechanism absent from build */
#define TC_DRBG_LIMIT TC_RESULT_LIMIT             /* request exceeds configured bound */
#define TC_DRBG_ENTROPY TC_RESULT_ENTROPY         /* entropy source failed */
#define TC_DRBG_ERROR TC_RESULT_ERROR             /* primitive failed; state destroyed */

/* Instantiation parameters. Zero-initialize, then set the fields for the
 * chosen mechanism. The security strength is the mechanism's maximum:
 * 128 bits for SHA-1, 192 for SHA-224, 256 for SHA-256/384/512, and the AES
 * key size for CTR_DRBG. */
typedef struct {
  TC_DRBG_mechanism mechanism;
  TC_hash_algorithm hash;        /* Hash_DRBG and HMAC_DRBG */
  uint8_t aes_key_bytes;         /* CTR_DRBG: 16, 24 or 32 */
  uint8_t derivation_function;   /* CTR_DRBG: 1 uses block_cipher_df */
  uint8_t prediction_resistance; /* 1 allows prediction-resistant requests */
  /* Entropy bytes requested per instantiate and reseed. 0 selects the
   * minimum: strength/8, or the seed length for CTR_DRBG without a
   * derivation function, which must use exactly that length. */
  size_t entropy_bytes;
  /* Generate requests allowed between reseeds. 0 selects 2^48. */
  uint64_t reseed_interval;
} TC_DRBG_config;

/* Working state. Treat every field as private. */
typedef struct {
  uint32_t marker;
  uint8_t mechanism, hash, key_bytes, seed_bytes, output_bytes;
  uint8_t derivation_function, prediction_resistance, failed;
  uint16_t strength_bits;
  size_t entropy_bytes;
  uint64_t reseed_counter, reseed_interval;
  TC_random_source entropy;
  uint8_t input[TC_DRBG_MAX_ENTROPY_BYTES]; /* entropy input, wiped after use */
  union {
    uint8_t unused;
#if TC_DRBG_HAVE_HASH
    struct {
      uint8_t v[TC_DRBG_HASH_SEED_BYTES], c[TC_DRBG_HASH_SEED_BYTES];
    } hash;
#endif
#if TC_DRBG_HAVE_HMAC
    struct {
      uint8_t key[TC_DRBG_HMAC_OUTPUT_BYTES], v[TC_DRBG_HMAC_OUTPUT_BYTES];
    } hmac;
#endif
#if TC_DRBG_HAVE_CTR
    struct {
      TC_AES_dynamic_key key;
      uint8_t v[16];
    } ctr;
#endif
  } state;
  union {
    uint8_t unused;
#if TC_DRBG_HAVE_HASH
    TC_hash_context hash;
#endif
#if TC_DRBG_HAVE_HMAC
    TC_HMAC_context hmac;
#endif
#if TC_DRBG_HAVE_CTR
    TC_AES_dynamic_key df_key; /* block_cipher_df key schedule */
#endif
  } scratch;
} TC_DRBG;

/* Instantiate (SP 800-90A section 9.1). entropy supplies entropy_bytes per
 * call, plus the nonce when nonce is empty. A nonce, when given, has at
 * least strength/16 bytes (section 8.6.7). CTR_DRBG without a derivation
 * function takes no nonce, and its personalization string is at most the
 * seed length. drbg keeps a copy of entropy, whose context must outlive the
 * instantiation. config, nonce and personalization are read only during
 * the call and must be disjoint from drbg.
 * Returns TC_DRBG_OK. TC_DRBG_ARGUMENT reports a NULL argument or fill
 * callback, an invalid span, an overlap, an input above the limits, a
 * config flag other than 0 or 1, a reseed_interval above 2^48, an unknown
 * mechanism, hash or AES key size, an entropy_bytes outside the mechanism
 * rules, or a nonce that is present when unused or too short.
 * TC_DRBG_UNSUPPORTED reports a mechanism or hash compiled out.
 * TC_DRBG_ENTROPY and TC_DRBG_ERROR report a failed source or primitive.
 * drbg may hold a previous instantiation, which is replaced. A NULL drbg is
 * left alone. On every other failure the whole context is wiped and left
 * uninstantiated, including on argument errors. */
TC_DRBG_result TC_DRBG_instantiate(TC_DRBG* drbg, const TC_DRBG_config* config,
                                   TC_random_source entropy, TC_bytes nonce,
                                   TC_bytes personalization);

/* Reseed with entropy_bytes of fresh entropy and optional additional input
 * (section 9.2). additional is read only during the call and must be
 * disjoint from drbg. Returns TC_DRBG_OK, TC_DRBG_ARGUMENT with the state
 * unchanged for an uninstantiated or failed drbg, an invalid span, an
 * overlap or an input above the limits, TC_DRBG_ENTROPY with the state
 * unchanged, or TC_DRBG_ERROR. */
TC_DRBG_result TC_DRBG_reseed(TC_DRBG* drbg, TC_bytes additional);

/* Fill output.capacity bytes (section 9.3), at most
 * TC_DRBG_MAX_REQUEST_BYTES. output.data may be NULL only when capacity is 0.
 * A prediction-resistant request reseeds first and needs a DRBG
 * instantiated with prediction_resistance. When the reseed interval is
 * exhausted, the DRBG reseeds itself from its entropy source. A reseed
 * consumes the additional input. Output, additional input and drbg must be
 * pairwise disjoint.
 * Returns TC_DRBG_OK, TC_DRBG_LIMIT for a longer request, TC_DRBG_ARGUMENT
 * for a NULL output with a nonzero length, an uninstantiated or failed
 * drbg, an invalid span, an overlap, an input above the limits or an
 * unavailable prediction-resistant request, TC_DRBG_ENTROPY for a failed
 * reseed with the state unchanged, or TC_DRBG_ERROR. ARGUMENT is checked
 * before LIMIT, and both leave the output and the state unchanged. ENTROPY
 * and ERROR wipe the output. */
TC_DRBG_result TC_DRBG_generate(TC_DRBG* drbg, TC_buffer output, int prediction_resistance,
                                TC_bytes additional);

/* Wipe the whole context (section 9.4). NULL is accepted. */
void TC_DRBG_uninstantiate(TC_DRBG* drbg);

/* TC_random_fn over an instantiated DRBG passed as user. Requests of any
 * length are split into TC_DRBG_MAX_REQUEST_BYTES generate calls without
 * additional input. Returns TC_OK when every byte was generated. An
 * argument error returns TC_ERROR before any byte is written, with output
 * and state unchanged: a NULL, uninstantiated or failed drbg, a NULL output
 * with a nonzero length, or output that overlaps the DRBG. Any other
 * generate failure returns TC_ERROR and wipes all length bytes of the
 * output. */
TC_status TC_DRBG_random(void* user, uint8_t* output, size_t length);

/* A TC_random_source over TC_DRBG_random for RSA key generation, EC, key
 * challenges and PIV secure messaging. The source borrows drbg, which must
 * outlive it. Calls sharing the source need application serialization. */
TC_random_source TC_DRBG_random_source(TC_DRBG* drbg);

#ifdef __cplusplus
}
#endif

#endif /* TC_ENABLE_DRBG */
#endif
